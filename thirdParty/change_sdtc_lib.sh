
dir="/usr/lib64"
unlink ${dir}/libstdc++.so.6
ln -s ${dir}/libstdc++.so.6.0.29 ${dir}/libstdc++.so.6
cd $dir
pwd
ls -ltr |grep stdc

dir="/usr/lib/gcc/x86_64-redhat-linux/8/32/"
unlink ${dir}/libstdc++.so
ln -s /usr/lib64/libstdc++.so.6.0.29 ${dir}/libstdc++.so
cd $dir
pwd
ls -ltr |grep stdc

dir="/usr/lib/gcc/x86_64-redhat-linux/8"
unlink ${dir}/libstdc++.so
ln -s /usr/lib64/libstdc++.so.6.0.29 ${dir}/libstdc++.so
cd $dir
pwd
ls -ltr |grep stdc
