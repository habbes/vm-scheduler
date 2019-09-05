#!/bin/sh

for i in 1 2 3 4 5 6 7 8
do
    echo "Creating vm$i"
    uvt-kvm create vm$i release=xenial
done

for i in 1 2 3 4 5 6 7 8
do
    echo "Waiting for vm$i"
    uvt-kvm wait vm$i
done

echo "Done. All VMs ready."