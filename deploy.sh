host='odin1@20.0.0.214'
path='gen4'

rsync -avz --exclude .git --filter=':- .gitignore' ./ $host:$path/

ssh $host "cd $path/app; premake5 gmake; cd build; make"
