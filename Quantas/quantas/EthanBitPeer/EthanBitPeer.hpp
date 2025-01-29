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

#ifndef ExamplePeer_hpp
#define ExamplePeer_hpp

#include "../Common/Peer.hpp"
#include "../Common/Simulation.hpp"
#include <iostream>
#include <string>
#include <vector>

namespace quantas {

using std::ostream;
using std::string;
using std::vector;

struct bitcoinTransaction {
    int id = -1;             // the transaction id
    int submittedRound = -1; // round in which the transaction had been
                             // submitted
};

struct bitcoinBlock {
    int minerID = -1;         // ID of the miner
    bitcoinTransaction trans; // transaction on the block
    int tipMiner = -1;        // ID of miner who mined previous block
    int length = 1;           // length of blockchain
};

struct bitcoinMessage {
    bitcoinBlock block; // whats being sent
    bool mined = false; // decides if its a block or a message
};

class EthanBitPeer : public Peer<bitcoinMessage> {
  public:
    EthanBitPeer(long);
    EthanBitPeer(const EthanBitPeer &rhs);
    EthanBitPeer();

    /*
    -blockChain function: keeps track of actual block chain
    -unlinked block function: unlinked blocks
    -transaction function: a vector of recieved transactions
    -submit rate
    -mine rate
    - id of next transaction to submit
        static int            currentTransaction;
        static mutex          currentTransaction_mutex;
    -// checkInStrm loops through the in stream adding blocks to unlinked or
    transactions void                  checkInStrm();
    // linkBlocks attempts to add unlinkedBlocks to the blockChain
    void                  linkBlocks();
    // guardSubmit checks if the node should submit a transaction
    bool                  guardSubmitTrans();
    // submitTrans creates a transaction and broadcasts it to everyone
    void                  submitTrans();
    // guardMineBlock determines if the node can mine a block
    bool 				  guardMineBlock();
    // mineBlock mines the next transaction, adds it to the blockChain and
    broadcasts it void                  mineBlock();
    // findNextTransaction finds the next unmined transaction on the longest
    chain of the blockChain BitcoinBlock          findNextTransaction();
    */
};

} // namespace quantas
#endif /* ExamplePeer_hpp */
