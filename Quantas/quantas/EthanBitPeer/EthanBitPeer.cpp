// /*
// Copyright 2022

// This file is part of QUANTAS.
// QUANTAS is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version. QUANTAS is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details. You should have received a copy of the GNU General Public License
// along with QUANTAS. If not, see <https://www.gnu.org/licenses/>.
// */

#include "EthanBitPeer.hpp"
#include <algorithm>
#include <iostream>
#include <random>
#include <unordered_map>
#include <vector>

namespace quantas {

using std::cout;
using std::endl;
using std::lock_guard;
using std::mt19937;
using std::mutex;
using std::random_device;
using std::uniform_int_distribution;

int EthanBitPeer::currentTransaction = 1;
mutex EthanBitPeer::currentTransaction_mutex;

EthanBitPeer::~EthanBitPeer() {}
EthanBitPeer::EthanBitPeer(const EthanBitPeer &rhs) {}
EthanBitPeer::EthanBitPeer(long id) : Peer(id) {}

void EthanBitPeer::performComputation() {
    checkIncomingMessages();
    if (checkSubmitTrans()) {
        submitTrans();
    }
    if (checkMineBlock()) {
        mineBlocks();
    }
}

void EthanBitPeer::endOfRound(const vector<Peer<bitcoinMessage> *> &_peers) {
    int blockChainLength =
        longestChain.size(); // Assuming longestChain is correctly updated
    cout << "End of round: Block Chain length: " << blockChainLength << endl;
    // Count the number of forks (where a parent block has multiple child
    // blocks)
    int forkCount = 0;
    for (const auto &edge : edges) {
        if (edge.second.size() > 1) {
            forkCount++;
        }
    }

    LogWriter::getTestLog()["Block Chain Length"].push_back(blockChainLength);
    LogWriter::getTestLog()["Forks: "].push_back(forkCount);
}

// void EthanBitPeer::endOfRound(const vector<Peer<bitcoinMessage> *> &_peers) {

//     int blockChainLength =
//         longestChain.size(); // Assuming longestChain is correctly updated

//     // 2. Count the number of forks (where a parent block has multiple child
//     // blocks)
//     int forkCount = 0;
//     for (const auto &edge : edges) {
//         if (edge.second.size() > 1) {
//             // Fork occurs when a parent block has multiple children
//             forkCount++;
//         }
//     }

//     LogWriter::getTestLog()["Block Chain
//     Length"].push_back(blockChainLength); LogWriter::getTestLog()["Forks:
//     "].push_back(forkCount);
// }

void EthanBitPeer::checkIncomingMessages() {
    while (!inStreamEmpty()) {
        Packet<bitcoinMessage> newMsg = popInStream();
        if (newMsg.getMessage().mined) {
            unlinkedBlocks.push_back(newMsg.getMessage().block);
        } else {
            transactions.push_back(newMsg.getMessage().block);
            cout << "Transaction added: Block ID "
                 << newMsg.getMessage().block.blockID << endl;
        }
    }
    linkBlocks();
}

// void EthanBitPeer::linkBlocks() {
//     vector<bitcoinBlock> stillUnlinked; // stores blocks that remain unlinked

//     // Iterate over the unlinked blocks
//     for (auto &block : unlinkedBlocks) {
//         int parentID = block.parentBlockID;

//         // Check if the parent block exists in the blocks map
//         if (blocks.find(parentID) != blocks.end()) {
//             // If parent block exists, link the current block
//             cout << "Linking block with ID" << block.blockID << " to parent
//             ID"
//                  << parentID << endl;
//             blocks[block.blockID] =
//                 block; // Add the current block to the blocks map

//             // Add the current block's ID to the edges list of the parent
//             block edges[parentID].push_back(block.blockID);

//             // Remove parent block ID from tips if it was a tip
//             tips.erase(parentID);

//             // Insert the current block's ID into the tips set
//             tips.insert(block.blockID);
//         } else {
//             // If the parent block doesn't exist, keep the current block
//             // unlinked
//             stillUnlinked.push_back(block);
//         }
//     }

//     // After finishing the block link processing, set remaining unlinked
//     blocks unlinkedBlocks = stillUnlinked;
// }

void EthanBitPeer::linkBlocks() {
    bool linked;
    do {
        linked = false;

        // Iterate over the unlinked blocks
        for (int i = 0; i < unlinkedBlocks.size(); i++) {
            bitcoinBlock block = unlinkedBlocks[i];
            int parentID = block.parentBlockID;

            // Check if the parent block exists in the blocks map
            if (blocks.find(parentID) != blocks.end()) {
                // If parent block exists, link the current block
                cout << "Linking block with ID " << block.blockID
                     << " to parent ID " << parentID << endl;
                blocks[block.blockID] =
                    block; // Add the current block to the blocks map

                // Add the current block's ID to the edges list of the parent
                // block
                edges[parentID].push_back(block.blockID);

                // Remove parent block ID from tips if it was a tip
                tips.erase(parentID);

                // Insert the current block's ID into the tips set
                tips.insert(block.blockID);

                // Remove the block from the unlinked list as it has been linked
                unlinkedBlocks.erase(unlinkedBlocks.begin() + i);
                linked = true;
                break; // Exit the loop once a block is successfully linked
            }
        }
    } while (linked); // Continue until no more blocks can be linked
}

// void EthanBitPeer::linkBlocks() {
//     vector<bitcoinBlock> stillUnlinked; // stores blocks that remain unlinked

//     // Iterate over the unlinked blocks
//     for (auto &block : unlinkedBlocks) {
//         int parentID = block.parentBlockID;

//         // Check if the parent block exists in the blocks map
//         if (blocks.find(parentID) != blocks.end()) {
//             // If parent block exists, link the current block
//             blocks[block.blockID] =
//                 block; // Add the current block to the blocks map

//             // Add the current block's ID to the edges list of the parent
//             block edges[parentID].push_back(block.blockID);

//             // Erase the parent block ID from the tips set, as it is no
//             longer a
//             // tip
//             tips.erase(parentID);

//             // Insert the current block's ID into the tips set
//             tips.insert(block.blockID);
//         } else {
//             // If the parent block doesn't exist, keep the current block
//             // unlinked
//             stillUnlinked.push_back(block);
//         }
//     }
// }

bool EthanBitPeer::checkSubmitTrans() { return randMod(submitRate) == 0; }

void EthanBitPeer::submitTrans() {
    if (!transactions.empty()) {
        bitcoinMessage msg;
        msg.block = transactions.back();
        msg.mined = false;

        broadcast(msg);
        transactions.pop_back();
    }
}

bool EthanBitPeer::checkMineBlock() { return randMod(mineRate) == 0; }

void EthanBitPeer::mineBlocks() {
    if (!transactions.empty()) {
        bitcoinBlock newBlock;
        newBlock.blockID = currentTransaction++;
        newBlock.parentBlockID = getLongestChainTip();
        cout << "Mining block with ID" << newBlock.blockID << "from parent"
             << newBlock.parentBlockID << endl;
        // Assign all the transactions from the transaction list
        newBlock.transaction =
            transactions[0].transaction; // Example, handle all transactions

        blocks[newBlock.blockID] = newBlock;

        tips.insert(newBlock.blockID);

        // Send mined block
        bitcoinMessage msg;
        msg.block = newBlock;
        msg.mined = true;
        broadcast(msg);
    } else {
        cout << "No transactions available to mine a block." << endl;
    }
}

bitcoinBlock EthanBitPeer::findNextTransaction() {
    if (transactions.empty()) {
        cout << "No transactions available" << endl;
        return bitcoinBlock(
        ); // Return an empty block if there are no transactions
    }

    // Get the next block from the transaction list
    bitcoinBlock nextBlock = transactions.back(); // Directly access the block
    transactions.pop_back(); // Remove the block from the list

    // Create a new block and assign necessary information
    nextBlock.blockID = currentTransaction++; // Assign a new block ID
    nextBlock.parentBlockID = getLongestChainTip(
    ); // Set parent block ID to the latest block in the chain

    cout << "Found next transaction with block ID: " << nextBlock.blockID
         << " and parent block id: " << nextBlock.parentBlockID << endl;
    // No need for 'transaction' member, as it is part of the block
    // The transaction is already contained in 'nextBlock'

    return nextBlock;
}

// void EthanBitPeer::updateLongestChain() {
//     std::vector<int> chain;
//     int currentTip = getLongestChainTip();

//     while (currentTip != -1) {
//         chain.push_back(currentTip);
//         if (edges.find(currentTip) != edges.end() &&
//             !edges[currentTip].empty()) {
//             currentTip = *edges[currentTip].rbegin();
//         } else {
//             break;
//         }
//     }

//     longestChain = chain;
// }

void EthanBitPeer::updateLongestChain() {
    std::vector<int> chain;
    int currentTip = getLongestChainTip();
    cout << "Updating longest chain for Peer ID: " << id() << endl;
    while (currentTip != -1) {
        chain.push_back(currentTip);
        if (edges.find(currentTip) != edges.end() &&
            !edges[currentTip].empty()) {
            currentTip = *edges[currentTip].rbegin();
        } else {
            break;
        }
    }

    longestChain = chain;
    cout << "Longest chain length for Peer id: " << id() << ": "
         << longestChain.size() << endl;
}

int EthanBitPeer::getLongestChainTip() const {
    // Return the tip (latest) of the longest chain
    return longestChain.empty() ? -1 : longestChain.back();
}

ostream &EthanBitPeer::printTo(ostream &os) const {
    os << "Peer ID: " << id() << " | Blocks: " << longestChain.size();
    return os;
}

ostream &operator<<(ostream &os, const EthanBitPeer &peer) {
    return peer.printTo(os);
}

Simulation<quantas::bitcoinMessage, quantas::EthanBitPeer> *generateSim() {
    Simulation<quantas::bitcoinMessage, quantas::EthanBitPeer> *sim =
        new Simulation<quantas::bitcoinMessage, quantas::EthanBitPeer>;
    return sim;
}
} // namespace quantas
// namespace quantas