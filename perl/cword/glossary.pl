while(<>){
	if (/size=\"2\"\>(.*?)\<\/font\>/) {
		print "$1\n";
	}
}