host='kiosk1@20.0.0.210'
path='evk4'

rsync -avz --exclude recordings --exclude .git --exclude build ./ $host:$path/

ssh $host "cd $path/app; premake5 gmake; cd build; make"
