

#echo "gcc -ggdb -fPIC -shared -o libArcAIClient.so ArcAIClient.c"
#gcc -ggdb  -fPIC -shared -o libArcAIClient.so ArcAIClient.c

#echo "gcc -fpic myLog.c"
#gcc -fpic -c myLog.c
#echo "gcc -shared -o myLog.so myLog.o"
#gcc -shared -o myLog.so myLog.o

echo "gcc -ggdb  -fPIC -L. -shared -o libArcAIClient.so ArcAIClient.c -laiISPLog -lailoglite"
gcc -ggdb  -fPIC -L. -shared -o libArcAIClient.so ArcAIClient.c -laiISPLog -lailoglite
