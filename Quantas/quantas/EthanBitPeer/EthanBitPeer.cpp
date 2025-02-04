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

    // Find the peer with the shortest blockchain
    int minLength = peers[0]->blockChains.size();
    int minIndex = 0;
    for (size_t i = 1; i < peers.size(); i++) {
        if (peers[i]->blockChains.size() < minLength) {
            minLength = peers[i]->blockChains.size();
            minIndex = static_cast<int>(i);
        }
    }
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
        if (linked) {
            it = unlinkedBlocks.erase(it);
        } else {
            ++it;
        }
    }
}

bool EthanBitPeer::checkSubmitTrans() {
    static mt19937 gen(random_device{}());
    return uniform_int_distribution<>(0, 99)(gen) < submitRate;
}

void EthanBitPeer::submitTrans() {
    lock_guard<mutex> lock(transactionMutex);

    bitcoinTransaction tx{currentTransactionID++, getRound(), isMalicious};

    int parentBlockID =
        blockChains.empty() ? -1 : blockChains.front().back().minerID;
    int chainLength = blockChains.empty() ? 1 : blockChains.front().size() + 1;

    bitcoinBlock block{id(), tx, parentBlockID, chainLength, isMalicious};

    bitcoinMessage msg{block, false};
    broadcast(msg);
}

bool EthanBitPeer::checkMineBlock() {
    static mt19937 gen(random_device{}());
    return uniform_int_distribution<>(0, 99)(gen) < mineRate;
}

void EthanBitPeer::mineBlock() {
    bitcoinBlock newBlock{
        id(), findNextTrans().transaction, blockChains.front().back().minerID,
        (int)blockChains.front().size() + 1, isMalicious
    };
    bitcoinMessage msg{newBlock, true};
    broadcast(msg);
}

bitcoinBlock EthanBitPeer::findNextTrans() {
    if (!transactions.empty()) {
        bitcoinBlock blk = transactions.back();
        transactions.pop_back();
        return blk;
    }
    return bitcoinBlock();
}

void EthanBitPeer::attemptDoubleSpend() {
    if (transactions.size() >= 2) {
        bitcoinTransaction maliciousTx = transactions.back().transaction;
        maliciousTx.isMalicious = true;
        bitcoinBlock maliciousBlock{
            id(), maliciousTx, blockChains.front().back().minerID,
            (int)blockChains.front().size() + 1, true
        };
        bitcoinMessage msg{maliciousBlock, true};
        broadcast(msg);
    }
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