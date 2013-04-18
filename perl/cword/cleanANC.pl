#!/usr/bin/perl

my $LIMIT = 20000;
my $count=0;

while(<>) {
	chomp;
	my $word = (split(/\t/))[0];
	lc;
	next if /[':]/;
	#next if length($word) < 2;
	next if ($count++ > $LIMIT);
	print "$word\n";
	
	# 
	# if ($count++ > $LIMIT) {
	# 	print "$word\n" if length($word) <3;
	# } else {
	# 	print "$word\n";
	# }
}