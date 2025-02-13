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

int EthanBitPeer::blockCounter = 0;
int EthanBitPeer::currentTransaction = 1;
mutex EthanBitPeer::currentTransaction_mutex;

EthanBitPeer::~EthanBitPeer() {}
EthanBitPeer::EthanBitPeer(const EthanBitPeer &rhs) : Peer(rhs) {}
EthanBitPeer::EthanBitPeer(long id) : Peer(id) {}

void EthanBitPeer::performComputation() {
    checkIncomingMessages();
    if (checkSubmitTrans()) {
        submitTrans();
    }
    if (checkMineBlock()) {
        mineBlocks();
    }
    updateLongestChain(); // Added to update the chain after each round
}

void EthanBitPeer::linkBlocks() {
    bool linked;
    do {
        linked = false;
        for (size_t i = 0; i < unlinkedBlocks.size(); i++) {
            bitcoinBlock block = unlinkedBlocks[i];
            int parentID = block.parentBlockID;

            if (parentID == -1) {
                cout << "DEBUG Linking genesis block" << block.blockID << endl;
                blocks[block.blockID] = block;
                edges[parentID].push_back(block.blockID);
                tips.insert(block.blockID);
                unlinkedBlocks.erase(unlinkedBlocks.begin() + i);
                linked = true;
                break;
            } else if (blocks.find(parentID) != blocks.end()) {
                cout << "DEBUG: Linking Block" << block.blockID << " to parent "
                     << parentID << endl;

                blocks[block.blockID] = block;
                edges[parentID].push_back(block.blockID);

                if (edges[parentID].size() > 1) {
                    cout << "DEBUG: Fork detected at block " << parentID
                         << "with " << edges[parentID].size() << " children"
                         << endl;
                }

                tips.erase(parentID);
                tips.insert(block.blockID);

                unlinkedBlocks.erase(unlinkedBlocks.begin() + i);
                linked = true;
                break;
            }
        }
    } while (linked);
}

void EthanBitPeer::endOfRound(const vector<Peer<bitcoinMessage> *> &_peers) {
    updateLongestChain();

    // Get statistics for this miner
    int minerSwitches = getTotalSwitches(id());
    int minerFlippedBlocks = getTotalFlippedBlocks(id());

    // Calculate network-wide statistics
    int totalNetworkSwitches = 0;
    int totalNetworkFlippedBlocks = 0;

    // Cast peers to EthanBitPeer to access their statistics
    for (const auto &peer : _peers) {
        const EthanBitPeer *bitPeer = static_cast<const EthanBitPeer *>(peer);
        totalNetworkSwitches += bitPeer->getTotalSwitches(bitPeer->id());
        totalNetworkFlippedBlocks +=
            bitPeer->getTotalFlippedBlocks(bitPeer->id());
    }

    LogWriter::getTestLog()["Block Chain Length"].push_back(longestChain.size()
    );
    LogWriter::getTestLog()["Chain Switches"].push_back(minerSwitches);
    LogWriter::getTestLog()["Flipped Blocks"].push_back(minerFlippedBlocks);
    LogWriter::getTestLog()["Network Total Switches"].push_back(
        totalNetworkSwitches
    );
    LogWriter::getTestLog()["Network Total Flipped Blocks"].push_back(
        totalNetworkFlippedBlocks
    );

    /* I want to track switches that occured per round this is essentially the
       number of forks per round also, the number of switches divided by peers
       in the network*/
}

void EthanBitPeer::checkIncomingMessages() {
    while (!inStreamEmpty()) {
        Packet<bitcoinMessage> newMsg = popInStream();
        if (newMsg.getMessage().mined) {
            cout << "DEBUG: Recieved mined block "
                 << newMsg.getMessage().block.blockID << " with parent "
                 << newMsg.getMessage().block.parentBlockID << endl;
            unlinkedBlocks.push_back(newMsg.getMessage().block);
            linkBlocks();
        } else {
            transactions.push_back(newMsg.getMessage().block);
            cout << "Transaction added" << endl;
        }
    }
}

bool EthanBitPeer::checkSubmitTrans() { return randMod(submitRate) == 0; }

void EthanBitPeer::submitTrans() {
    const lock_guard<mutex> lock(currentTransaction_mutex);
    bitcoinMessage msg;
    msg.mined = false;
    msg.block.transaction.id = currentTransaction++;
    msg.block.transaction.roundSubmitted = getRound();
    broadcast(msg);
}

bool EthanBitPeer::checkMineBlock() { return randMod(mineRate) == 0; }

void EthanBitPeer::mineBlocks() {
    if (!transactions.empty()) {
        bitcoinBlock newBlock;

        if (tips.empty()) {
            newBlock.parentBlockID = -1; // genesis block
        } else {
            auto it = tips.begin();
            std::advance(it, randMod(tips.size()));
            newBlock.parentBlockID = *it;
        }

        newBlock.blockID = blockCounter++;
        newBlock.transaction = transactions.front().transaction;
        transactions.erase(transactions.begin());

        cout << "DEBUG: Mining new block " << newBlock.blockID
             << " with parent " << newBlock.parentBlockID << endl;

        blocks[newBlock.blockID] = newBlock;
        edges[newBlock.parentBlockID].push_back(newBlock.blockID);

        if (newBlock.parentBlockID != -1) {
            tips.erase(newBlock.parentBlockID);
        }
        tips.insert(newBlock.blockID);

        bitcoinMessage msg;
        msg.block = newBlock;
        msg.mined = true;
        broadcast(msg);
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

void EthanBitPeer::updateLongestChain() {
    vector<int> newLongestChain;

    for (int tip : tips) {
        vector<int> currentPath;
        int currentBlockID = tip;

        while (currentBlockID != -1) {
            currentPath.push_back(currentBlockID);
            auto it = blocks.find(currentBlockID);
            if (it != blocks.end()) {
                currentBlockID = it->second.parentBlockID;
            } else {
                break;
            }
        }

        reverse(currentPath.begin(), currentPath.end());

        if (currentPath.size() > newLongestChain.size()) {
            newLongestChain = currentPath;
        }
    }

    if (newLongestChain != longestChain) {
        trackChainSwitch(longestChain, newLongestChain);
    }

    longestChain = newLongestChain;
    longestChainLength = longestChain.size();

    minerStats[id()].currentChain = longestChain;
}

void EthanBitPeer::trackChainSwitch(
    const vector<int> &oldChain, const vector<int> &newChain
) {
    if (oldChain.empty() || newChain.empty()) {
        return;
    }

    int flippedBlocks = countFlippedBlocks(oldChain, newChain);

    if (flippedBlocks > 0) {
        minerStats[id()].totalSwitches++;
        minerStats[id()].totalFlippedBlocks += flippedBlocks;

        minerStats[id()].switchHistory.push_back(oldChain);

        cout << "DEBUG: Miner " << id() << " switch chains." << flippedBlocks
             << " blocks were flipped." << endl;
    }
}

int EthanBitPeer::countFlippedBlocks(
    const vector<int> &oldChain, const vector<int> &newChain
) {
    size_t i = 0;
    while (i < oldChain.size() && i < newChain.size() &&
           oldChain[i] == newChain[i]) {
        i++;
    }

    return oldChain.size() - i;
}

int EthanBitPeer::getLongestChainTip() const {
    // Return the tip (latest) of the longest chain
    return longestChain.empty() ? -1 : longestChain.back();
}

int EthanBitPeer::getTotalSwitches(long minerId) const {
    auto it = minerStats.find(minerId);
    return (it != minerStats.end()) ? it->second.totalSwitches : 0;
}

int EthanBitPeer::getTotalFlippedBlocks(long minerId) const {
    auto it = minerStats.find(minerId);
    return (it != minerStats.end()) ? it->second.totalFlippedBlocks : 0;
}

const vector<int> &EthanBitPeer::getCurrentChain(long minerId) const {
    static const vector<int> emptyChain;
    auto it = minerStats.find(minerId);
    return (it != minerStats.end()) ? it->second.currentChain : emptyChain;
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