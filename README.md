# Code for "Towards Surrogate Modeling of the Hydrodynamics of Historic Canals"
Repository for surrogate model libraries and numerical model files used in Chapters 6-9 of the senior thesis "Towards Surrogate Modeling of the Hydrodynamics of Historic Canals" by Justin Cai. Software used were OpenFOAM v2312, Python 3.6, and TensorFlow C API 2.15.0 for NN_Pred and 1.15.0 for TensorFlowFoam.
## NN_Pred
Library by W. Liu, Z. Song, and Fang (2023), https://www.sciencedirect.com/science/article/pii/S0010465523001200. See internal README for details on library and environment setup.
The NN_Pred library was deployed in Chapter 8: "Improved Iterative ML-RANS Turbulence Surrogate".
The Yamashina model case files and simulation results are in the directory NN_Pred/OpenFOAM-Extension/tutorials/turbulent_channel/Cases/YamashinaClosedNN
## PythonFOAM
Library by Maulik, Fytanidis et al. (2022), https://www.sciencedirect.com/science/article/abs/pii/S1877750322001387. See internal README for details on library and environment setup.
The PythonFOAM library was deployed in Chapter 9: "General-Purpose Data-Driven Turbulence Surrogates".
The Yamashina model case files and simulation results are in the directory PythonFOAM/TurbulenceModel_Examples/PysimpleFoam/YamashinaClosedPython
## TensorFlowFoam
Library by Maulik, Sharma, et al. (2020), https://arc.aiaa.org/doi/abs/10.2514/6.2021-1485. See internal README for details on library and environment setup.
The TensorFlowFoam library was deployed in Chapter 7: "RANS Turbulent Eddy-Viscosity Surrogate".
The Yamashina model case files and simulation results are in the directory TensorFlowFoam/ML_RANS/YamashinaClosedTF
## YamashinaClosed
Case files and results of simpleFoam with kOmegaSST turbulence model (Chapter 6 6.1-6.4). To reproduce results of kEpsilon model in section 6.5, modify YamashinaClosed/constant/turbulenceProperties line 21 to "RASModel    kEpsilon" and run the simulation.
## YamashinaClosedEmpty
Empty case files with Yamashina mesh and initial conditions ready to run a numerical solver.
