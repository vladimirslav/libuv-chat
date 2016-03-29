rm -rf Build_Server
mkdir Build_Server
cd Build_Server
cmake ../Server -DCMAKE_BUILD_TYPE=$1
make
