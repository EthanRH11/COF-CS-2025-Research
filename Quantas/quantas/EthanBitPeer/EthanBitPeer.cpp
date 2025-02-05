/*
Copyright 2022

This file is part of QUANTAS.
QUANTAS is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. QUANTAS is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with
QUANTAS. If not, see <https://www.gnu.org/licenses/>.
*/

#include "EthanBitPeer.hpp"
#include <iostream>
#include <random>

namespace quantas {

using std::cout;
using std::endl;
using std::lock_guard;
using std::mt19937;
using std::mutex;
using std::random_device;
using std::uniform_int_distribution;

std::unordered_set<int> quantas::EthanBitPeer::minedTransactionIDs;

int EthanBitPeer::currentTransactionID = 0;
mutex EthanBitPeer::transactionMutex;

EthanBitPeer::EthanBitPeer(long id) : Peer(id) {}

EthanBitPeer::EthanBitPeer(const EthanBitPeer &rhs) : Peer(rhs) {}

EthanBitPeer::~EthanBitPeer() {}

// void quantas::EthanBitPeer::endOfRound(
//     const vector<Peer<bitcoinMessage> *> &_peers
// ) {
//     std::vector<EthanBitPeer *> peers;
//     for (auto peer : _peers) {
//         peers.push_back(static_cast<EthanBitPeer *>(peer));
//     }

//     // Find the peer with the longest blockchain
//     int maxLength = 0;
//     for (size_t i = 0; i < peers.size(); i++) {
//         if (!peers[i]->blockChains.empty() &&
//             peers[i]->blockChains.front().size() > maxLength) {
//             maxLength = peers[i]->blockChains.front().size();
//         }
//     }

//     LogWriter::getTestLog()["blockChain_length"].push_back(maxLength);

//     LogWriter::getTestLog()["forks"].push_back(forkCount);

//     int totalMessagesSent = 0;
//     for (size_t i = 1; i < peers.size(); i++) {
//         totalMessagesSent += peers[i]->messagesSent;
//     }
//     LogWriter::getTestLog()["totalMessagesSent"].push_back(totalMessagesSent);
// }

void quantas::EthanBitPeer::endOfRound(
    const vector<Peer<bitcoinMessage> *> &_peers
) {
    std::vector<EthanBitPeer *> peers;
    for (auto peer : _peers) {
        peers.push_back(static_cast<EthanBitPeer *>(peer));
    }

    int totalMinedBlocks = 0;
    int maxLength = 0;
    int roundForkCount = 0;

    // Iterate through peers to track the longest chain and total mined blocks
    for (auto *peer : peers) {
        if (!peer->blockChains.empty()) {
            int currentChainLength = peer->blockChains.front().size();
            totalMinedBlocks += currentChainLength;

            // Check if the current chain is the longest
            if (currentChainLength > maxLength) {
                maxLength = currentChainLength;
            }

            // Count forks for this peer
            if (peer->blockChains.size() > 1) {
                roundForkCount += peer->blockChains.size() - 1;
            }
        }
    }

    // Logging fork count and mined blocks
    LogWriter::getTestLog()["minedBlocksMinusLongestChain"].push_back(
        totalMinedBlocks - maxLength
    );
    LogWriter::getTestLog()["blockChain_length"].push_back(maxLength);
    LogWriter::getTestLog()["forks"].push_back(roundForkCount);

    // Debugging output
    std::cout << "Total Mined Blocks: " << totalMinedBlocks << "\n";
    std::cout << "Max Chain Length: " << maxLength << "\n";
    std::cout << "Fork Count: " << roundForkCount << "\n";
    std::cout << "Mined Blocks - Longest Chain: "
              << totalMinedBlocks - maxLength << "\n";
}

void EthanBitPeer::performComputation() {
    checkIncomingMessages();

    if (checkSubmitTrans()) {
        submitTrans();
    }
    if (checkMineBlock()) {
        mineBlock();
    }
}

void EthanBitPeer::checkIncomingMessages() {
    while (!inStreamEmpty()) {
        Packet<bitcoinMessage> newMsg = popInStream();
        bitcoinMessage msg = newMsg.getMessage(); // extract actual message

        if (msg.mined) {
            unlinkedBlocks.push_back(msg.block);
        } else {
            transactions.push_back(msg.block);
        }
    }
}
void EthanBitPeer::linkBlocks() {
    for (auto it = unlinkedBlocks.begin(); it != unlinkedBlocks.end();) {
        bool linked = false;
        for (auto &chain : blockChains) {
            if (!chain.empty() && chain.back().minerID == it->parentBlockID) {
                chain.push_back(*it);
                linked = true;
                break;
            }
        }
        if (!linked && blockChains.empty()) {
            // If no chains exist, create a new chain with this block
            blockChains.push_back({*it});
            linked = true;
        }
        if (linked) {
            it = unlinkedBlocks.erase(it);
        } else {
            ++it;
        }
    }
}

bool EthanBitPeer::checkSubmitTrans() { return randMod(100) < submitRate; }

void EthanBitPeer::submitTrans() {
    lock_guard<mutex> lock(transactionMutex);

    bitcoinTransaction tx{currentTransactionID++, getRound(), isMalicious};

    // Prevent duplicate transactions
    if (minedTransactionIDs.count(tx.id) > 0) {
        // cout << "Duplicate transaction detected: " << tx.id << endl;
        return;
    }

    minedTransactionIDs.insert(tx.id); // Mark as mined

    int parentBlockID = -1; // Default for genesis block
    int chainLength = 1;    // Default for genesis block

    if (!blockChains.empty() && !blockChains.front().empty()) {
        parentBlockID = blockChains.front().back().minerID;
        chainLength = blockChains.front().size() + 1;
    }

    bitcoinBlock block{id(), tx, parentBlockID, chainLength, isMalicious};

    bitcoinMessage msg{block, false};
    messagesSent += neighbors().size();
    broadcast(msg);
}

bool EthanBitPeer::checkMineBlock() {
    int randVal = randMod(100);
    // cout << "Mine check: " << randVal << " < " << mineRate << endl;
    return randVal < mineRate;
    // return randMod(100) < mineRate;
}

// void EthanBitPeer::mineBlock() {
//     bitcoinBlock newBlock = findNextTrans();

//     // Ensure a valid transaction exists before mining
//     if (newBlock.transaction.id == 0) {
//         return;
//     }

//     // Check for a fork condition: multiple blocks with the same parent
//     int parentBlockID = -1; // Default for genesis block
//     int chainLength = 1;    // Default for genesis block

//     if (!blockChains.empty() && !blockChains.front().empty()) {
//         parentBlockID = blockChains.front().back().minerID;
//         chainLength = blockChains.front().size() + 1;
//     }
//     bool forkDetected = false;
//     // Check if a block with the same parent exists already
//     for (auto &chain : blockChains) {
//         if (!chain.empty() && chain.back().minerID == parentBlockID) {
//             // A fork detected, increment fork count
//             forkDetected = true;
//             forkCount++;
//             break;
//         }
//     }

//     // Add the new block to the blockchain (it may still create a valid
//     chain) newBlock.parentBlockID = parentBlockID; newBlock.length =
//     chainLength;

//     if (blockChains.empty()) {
//         blockChains.push_back({newBlock});
//     } else {
//         blockChains.front().push_back(newBlock);
//     }

//     // Broadcast the new block
//     bitcoinMessage msg{newBlock, true};
//     messagesSent += neighbors().size();
//     broadcast(msg);
// }

void EthanBitPeer::mineBlock() {
    bitcoinBlock newBlock = findNextTrans();

    if (newBlock.transaction.id == 0) {
        return;
    }

    int parentBlockID = -1;
    int chainLength = 1;

    // Ensure there is at least one chain to find the parent block
    if (!blockChains.empty() && !blockChains.front().empty()) {
        parentBlockID =
            blockChains.front().back().minerID; // Assuming the first chain
        chainLength = blockChains.front().size() + 1;
    }

    bool forkDetected = false;

    // Check if this block creates a fork by checking if the parent block
    // already exists in any chain
    for (auto &chain : blockChains) {
        if (!chain.empty() && chain.back().minerID == parentBlockID) {
            // Fork detected, increment fork count
            forkDetected = true;
            break;
        }
    }

    // Add the new block to the chain
    newBlock.parentBlockID = parentBlockID;
    newBlock.length = chainLength;

    if (forkDetected) {
        // Add this block to a new chain (indicating a fork)
        blockChains.push_back({newBlock});
        forkCount++; // Increment fork count for this round
    } else {
        // No fork detected, add to the main chain
        if (blockChains.empty()) {
            blockChains.push_back({newBlock});
        } else {
            blockChains.front().push_back(newBlock);
        }
    }

    // Broadcast the new block
    bitcoinMessage msg{newBlock, true};
    messagesSent += neighbors().size();
    broadcast(msg);
}

bitcoinBlock EthanBitPeer::findNextTrans() {
    // std::cout << "Transactions available: " << transactions.size() << endl;
    if (!transactions.empty()) {
        bitcoinBlock blk = transactions.back();
        transactions.pop_back();
        return blk;
    }
    // cout << "No transactions found!" << endl;
    return bitcoinBlock();
}

ostream &EthanBitPeer::printTo(ostream &os) const {
    os << "Peer ID: " << id() << " | Blocks: " << blockChains.front().size();
    return os;
}

ostream &operator<<(ostream &os, const EthanBitPeer &peer) {
    return peer.printTo(os);
}

// void EthanBitPeer::resolveForks() {
//     int longestChainLength = 0;
//     size_t longestChainIndex = 0;

//     // Find the longest chain
//     for (size_t i = 0; i < blockChains.size(); ++i) {
//         if (blockChains[i].size() > longestChainLength) {
//             longestChainLength = blockChains[i].size();
//             longestChainIndex = i;
//         }
//     }

//     // Remove other chains that are shorter than the longest one
//     for (size_t i = 0; i < blockChains.size(); ++i) {
//         if (i != longestChainIndex) {
//             blockChains[i].clear(); // Remove shorter chains
//         }
//     }

//     // Reset fork count after resolution
//     forkCount = 0;
// }

void EthanBitPeer::resolveForks() {
    int longestChainLength = 0;
    size_t longestChainIndex = 0;

    // Find the longest chain
    for (size_t i = 0; i < blockChains.size(); ++i) {
        if (blockChains[i].size() > longestChainLength) {
            longestChainLength = blockChains[i].size();
            longestChainIndex = i;
        }
    }

    // Remove other chains that are shorter than the longest one
    for (size_t i = 0; i < blockChains.size(); ++i) {
        if (i != longestChainIndex) {
            blockChains[i].clear(); // Remove shorter chains
        }
    }

    // Reset fork count after resolution
    forkCount = 0;
}

Simulation<bitcoinMessage, EthanBitPeer> *generateSim() {
    return new Simulation<bitcoinMessage, EthanBitPeer>();
}
} // namespace quantas