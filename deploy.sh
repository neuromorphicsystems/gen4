host='kiosk1@20.0.0.210'
path='evk4'

clang-format -i app/*.hpp app/*.cpp

rsync -avz --exclude build --exclude .git ./ $host:$path/
ssh $host "cd $path/app && premake5 gmake && cd build && make"
