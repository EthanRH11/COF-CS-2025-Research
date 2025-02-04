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

#ifndef EthanBitPeer_hpp
#define EthanBitPeer_hpp

#include "../Common/Peer.hpp"
#include "../Common/Simulation.hpp"
#include <mutex>
#include <vector>

namespace quantas {

using std::mutex;
using std::ostream;
using std::vector;

// Transaction Structure
struct bitcoinTransaction {
    long int id = -1;
    int roundSubmitted = -1;
    bool isMalicious = false; // Marks if transaction is malicious
};

// Block Structure
struct bitcoinBlock {
    long int minerID = -1;          // ID of peer who mined this block
    bitcoinTransaction transaction; // Transaction included in the block
    int parentBlockID = -1;         // Reference to parent (previous) block
    int length = 1;                 // Chain length (longest chain rule)
    bool isMalicious = false;       // Is block part of an attack
};

// Message Structure
struct bitcoinMessage {
    bitcoinBlock block; // Block being sent
    bool mined = false; // True if mined block, false if transaction
};

class EthanBitPeer : public Peer<bitcoinMessage> {
  public:
    EthanBitPeer(long id);
    EthanBitPeer(const EthanBitPeer &rhs);
    ~EthanBitPeer();

    void performComputation(); // Process transactions and mine blocks
    void endOfRound(const vector<Peer<bitcoinMessage> *> &_peers
    ); // Runs at end of each round

    void log() const { printTo(*_log); };
    ostream &printTo(ostream &) const;
    friend ostream &operator<<(ostream &, const EthanBitPeer &);

    // Block Chain & Fork Tracks Functions
    vector<vector<bitcoinBlock>> blockChains{
        {vector<bitcoinBlock>{bitcoinBlock()}}
    };
    vector<bitcoinBlock> unlinkedBlocks;
    vector<bitcoinBlock> transactions;

    // Sim Params
    int submitRate = 20;      // probability of submitting a transaction
    int mineRate = 40;        // probability of mining a block
    bool isMalicious = false; // True if this peer is attacking

    // Transaction ID manage
    static int currentTransactionID;
    static mutex transactionMutex;

    // BlockChain Operations
    void checkIncomingMessages(); // process messages
    void linkBlocks();            // Connect unlinked blocks
    bool checkSubmitTrans();      // Check if transaction should be submitted
    void submitTrans();           // Broadcast a new transaction
    bool checkMineBlock();        // check if mining is possible
    void mineBlock();             // create and broadcast a new block
    bitcoinBlock findNextTrans(); // Find an unmined transaction

    // Attack sim
    void attemptDoubleSpend(); // Malicious Peer
};

Simulation<bitcoinMessage, EthanBitPeer> *generateSim();

} // namespace quantas

#endif // EthanPeer_HPP