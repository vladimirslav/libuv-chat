rm -rf Build_Client
mkdir Build_Client
cd Build_Client
cmake ../Client -DCMAKE_BUILD_TYPE=$1
make
