### GPU Tarnsport Traffic Simulation ###
Project is developed with participation of **ITMO University** under the **ITMO Stars** program.
This project presents a new approach to **Real-Time** simulation of transport agents for **video games**. Most of existing approaches in city building games and are based on multi-threaded CPU utilization. However with huge advancements in perfomance of graphics adaptters we see an opportunity to utilize **GPU Compute** for maximizing performance of simulation.

Key features of GPU approach:
- **High parallelization**, execution of thousands threads, simulating behaviour of each agent simultaneously;
- **Reduced usage of PCIe bandwidth**, agent data is stored and modified on GPU only;
- **Data oriented by default**, everything is stored by type where you can reason on data access to improve performance.
