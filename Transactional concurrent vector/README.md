#### Steps to setup and run the transactional vector:
1. Download the RSTM library from the below link.

	https://code.google.com/archive/p/rstm/downloads
2. Unzip the tar file
3. Cd to the directory with the unpacked files
4. Copy the main.cpp file provided in the source code folder in the following directory.

	`rstm/bench/`
5. Create a new directory adjacent to the rstm directory

	`mkdir rstm_build`
6. Cd to the rstm_build directory
7. Build the rstm library using the make command

	`make`
8. Run the file using the below command

	  `bench/mainSSB64`
