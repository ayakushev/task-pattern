program.exe "foo %%{1} is a %%{1} bar" <check_indx.txt 2>log_check_indx.txt
program.exe "is %%{1S3} %%{2}a cat" <check_simple.txt 2>log_check_simple.log
program.exe "bar %%{0G} foo %%{1}" <check_G.txt 2>log_g.txt