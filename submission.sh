FOLDER=$1_$2_p1
TARGET_ZIP=$FOLDER.zip

rm $TARGET_ZIP

cd cpu && make clean
cd ..
cd memory && make clean
cd ..
zip -r $TARGET_ZIP cpu memory

echo "Generated submission file: $TARGET_ZIP"

