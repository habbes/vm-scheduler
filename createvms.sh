#!/bin/sh

echo "Creating $1 vms..."

for i in `seq $1`
do
    echo "Creating vm$i"
    uvt-kvm create vm$i release=xenial
done

for i in `seq  $1`
do
    echo "Waiting for vm$i"
    uvt-kvm wait vm$i
done

echo "Done. All VMs ready."