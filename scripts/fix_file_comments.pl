#!/usr/bin/perl


use File::Find;
use File::Basename;

find(
    {
        wanted => \&findfiles,
    },
    'include'
);

find(
    {
        wanted => \&findfiles,
    },
    'src'
);

find(
    {
        wanted => \&findfiles,
    },
    'examples'
);

sub findfiles {

    $header = <<EOH;
/*
 * sst-clap_helpers - an open source library of stuff which makes
 * making clap easier for the Surge Synth Team.
 *
 * Copyright 2023-2025, various authors, as described in the GitHub
 * transaction log.
 *
 * sst-clap-helpers is released under the MIT license, as described
 * by "LICENSE.md" in this repository.
 *
 * All source in sst-jucegui available at
 * https://github.com/surge-synthesizer/sst-clap-helpers
 */
EOH

    $f = $File::Find::name;
    if ($f =~ m/\.h$/ or $f =~ m/.cpp$/) {
        #To search the files inside the directories
        print "Processing $f\n";

        $q = basename($f);
        print "$q\n";
        open(IN, "<$q") || die "Cant open IN $!";
        open(OUT, "> ${q}.bak") || die "Cant open BAK $!";

        $nonBlank = 0;
        $inComment = 0;
        while (<IN>) {
            if ($nonBlank) {
                print OUT
            }
            else {
                if (m:^\s*/\*:) {
                    $inComment = 1;
                }
                elsif (m:\s*\*/:) {
                    print OUT $header;
                    $nonBlank = true;
                    $inComment = false;
                }
                elsif ($inComment) {

                }
                elsif (m:^//:) {

                }
                else {
                    print OUT $header;
                    $nonBlank = true;
                    print OUT;

                }
            }
        }
        close(IN);
        close(OUT);
        system("mv ${q}.bak ${q}");
        system("clang-format -i ${q}");
    }
}