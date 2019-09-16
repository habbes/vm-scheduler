#!/bin/sh

echo "Destroying $1 vms..."

for i in `seq $1`
do
    echo "Destroying vm$i"
    uvt-kvm destroy vm$i
done

echo "Done. All VMs ready."