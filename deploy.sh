host='odin@192.168.55.1'
path='evk4'

rsync -avz --exclude recordings --exclude .git --exclude build ./ $host:$path/

#printf "compile headless_recorder\n"
#ssh $host "cd $path/app; g++ -Wall -O3 headless_recorder.cpp -o headless_recorder -lpthread -L/usr/local/lib -lusb-1.0"

#printf "compile lsevk4\n"
#ssh $host "cd $path/app; g++ -Wall -O3 lsevk4.cpp -o lsevk4 -lpthread -L/usr/local/lib -lusb-1.0"

printf "compile bare_bones\n"
ssh $host "cd $path/app; g++ -Wall -O3 -I/usr/local/include -L/usr/local/lib bare_bones.cpp -o bare_bones -lpthread /usr/local/lib/libusb-1.0.a -ludev"
