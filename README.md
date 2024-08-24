# Performance Improvement of Multicore Processors Using Wireless On-chip Interconnects (WiNoC)

## Project Overview

This repository contains code and documentation for the research project on improving the performance of multicore processors using Wireless On-chip Interconnects (WiNoC). The focus is on integrating wireless features into the Garnet network model in gem5 to simulate and analyze performance enhancements in multicore systems.

## Key Features

- **Wireless Integration:**
  - Added wireless features to the Garnet network by modifying internal links to act as broadcasting links, mimicking wireless communication.

- **Routing Algorithm:**
  - Implemented a hybrid routing algorithm that selects between traditional XY routing and a wireless path based on hop count.
  - The routing decision is based on the hop count, determining whether to use the wireless or wired path to optimize performance.

- **Token Passing Mechanism:**
  - Incorporated static token passing for controlling broadcasting.
  - The token passing mechanism ensures orderly access to the wireless medium by regulating which router can transmit data at any given time.

## Current Challenges

- **Packet Latency:**
  - Observed high packet latency due to the concentration of packets at the wireless router.
  - Packets are queued for extended periods before finding their desired outport and continuing their flow.

## Future Work

- **Performance Optimization:**
  - Further work is needed to address high packet latency issues.
  - Plan to explore optimizations to reduce latency and improve overall network performance.

## Installation and Setup

1. **Clone the Repository:**

   ```bash
   git clone https://github.com/yourusername/your-repo-name.git
   cd your-repo-name
   ```

2. **Build gem5:**

   Follow the standard gem5 build instructions for your architecture to compile with the new Garnet network features.

   ```bash
   scons build/X86/gem5.opt -j4
   ```

3. **Run Simulations:**

   Execute your simulations using the updated Garnet network configuration to test the wireless features and performance.

## Token Passing Mechanism

The static token passing mechanism is used to control access to the wireless medium. Here’s a brief explanation:

- **Token Passing:** 
  - A token is a special packet that circulates among routers.
  - Only the router holding the token is allowed to broadcast data, ensuring organized and collision-free communication.
  - This approach helps in managing access to the wireless channel and prevents simultaneous transmission attempts that could lead to conflicts.

## Note

- The project is ongoing, and updates will be provided as further progress is made.
- Stay tuned for more improvements and optimizations.

## Contact

For questions or contributions, please reach out to [your email or contact information].

---

Feel free to modify the content to better fit your specific project details or personal preferences!
