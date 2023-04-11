for file in $(ls -Sr | head --lines $1); do
	lang=${file%.*};
	echo -en $lang'\t';
	wikt-lang.lua $lang;
	sd '\n' '\t' $file;
	echo;
done