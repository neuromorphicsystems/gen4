host='odin1@20.0.0.214'
path='evk4'

rsync -avz --exclude .git --filter=':- .gitignore' ./ $host:$path/

#ssh $host "cd $path/app; premake5 gmake; cd build; make"
