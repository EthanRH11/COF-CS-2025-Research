# Value Dependent Blockchain Confirmation: A Probabilistic Approach to Bitcoin Transaction Finality
## Project Overview
This simulator analyzes blockchain confirmation security by determining the optimal number of blocks a user should wait before considering a transaction irreversible. The model balances the value of the transaction against the risk of blockchain reorganization (reorg), helping users make informed decisions to protect their funds.

## Problem Statement
When receiving cryptocurrency payments, users face uncertainty about when to consider a transaction "final". Waiting for more confirmations increases security but adds time costs, while accepting transactions too quickly increases the risk of double-spend attacks through blockchain reorganizations.
This project quantifies this security-convenience tradeoff based on:

The monetary value of the transaction
The user's risk tolerance
Network conditions and potential attacker capabilities

## Core Features

Blockchain Simulation: A C++ implementation of a blockchain network with configurable parameters ([Quantas Sim](https://github.com/QuantasSupport/Quantas))
Reorg Probability Model: Statistical models calculating the probability of blockchain reorganizations at different depths
Value-Based Security Analysis: Framework for determining the optimal confirmation threshold based on transaction value (Prospect Theory)


## Technical Implementation
The simulator is built in C++ with the following components:

Blockchain structure with customizable consensus rules
Network propagation simulation
Mining difficulty adjustment algorithm
Statistical analysis tools for security evaluation

## Key Parameters

mine_rate: how fast blockchain miners can mine
submit_rate: how fast transactions are submitted
delay: How fast miners can communicate in a round
number_of_peers: How many miners are within the network

## Research Findings
Our simulations demonstrate that:

Small-value transactions (<$100) often require fewer confirmations (1-2 blocks)
High-value transactions benefit significantly from additional waiting time
Network conditions significantly impact optimal waiting periods
The security/time tradeoff follows a logarithmic rather than linear relationship

## Future Work

Implementation of more sophisticated attacker strategies
Economic models incorporating opportunity costs of waiting
Integration with real-time network data
Mobile application for on-the-fly security recommendations
