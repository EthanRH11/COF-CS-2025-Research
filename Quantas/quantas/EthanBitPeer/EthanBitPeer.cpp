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

void quantas::EthanBitPeer::endOfRound(
    const vector<Peer<bitcoinMessage> *> &_peers
) {
    std::vector<EthanBitPeer *> peers;
    for (auto peer : _peers) {
        peers.push_back(static_cast<EthanBitPeer *>(peer));
    }

    // Find the peer with the longest blockchain
    int maxLength = 0;
    for (size_t i = 0; i < peers.size(); i++) {
        if (!peers[i]->blockChains.empty() &&
            peers[i]->blockChains.front().size() > maxLength) {
            maxLength = peers[i]->blockChains.front().size();
        }
    }

    LogWriter::getTestLog()["blockChain_length"].push_back(maxLength);

    LogWriter::getTestLog()["forks"].push_back(0);

    int totalMessagesSent = 0;
    for (size_t i = 1; i < peers.size(); i++) {
        totalMessagesSent += peers[i]->messagesSent;
    }
    LogWriter::getTestLog()["totalMessagesSent"].push_back(totalMessagesSent);
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

void EthanBitPeer::mineBlock() {
    // cout << "Attempting to mine block..." << endl;

    bitcoinBlock newBlock = findNextTrans();

    // Ensure a valid transaction exists before mining
    if (newBlock.transaction.id == 0) {
        // cout << "No valid transaction for mining!" << endl;
        return;
    }

    // Assign correct parentBlockID and length
    int parentBlockID = -1; // Default for genesis block
    int chainLength = 1;    // Default for genesis block

    if (!blockChains.empty() && !blockChains.front().empty()) {
        parentBlockID = blockChains.front().back().minerID;
        chainLength = blockChains.front().size() + 1;
    }

    newBlock.parentBlockID = parentBlockID;
    newBlock.length = chainLength;

    // Add mined block to blockchain
    if (blockChains.empty()) {
        blockChains.push_back({newBlock});
    } else {
        blockChains.front().push_back(newBlock);
    }

    // cout << "Mined block with transaction ID: " << newBlock.transaction.id
    //      << ", New blockchain length: " << blockChains.front().size() <<
    //      endl;

    // Broadcast mined block
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

Simulation<bitcoinMessage, EthanBitPeer> *generateSim() {
    return new Simulation<bitcoinMessage, EthanBitPeer>();
}
} // namespace quantas