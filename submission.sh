FOLDER=$1_$2_p1
TARGET_ZIP=$FOLDER.zip

rm $TARGET_ZIP

zip -r $TARGET_ZIP cpu memory

echo "Generated submission file: $TARGET_ZIP"

