# VM Scheduler

This project contains implementation of a basic [VM CPU scheduler](/cpu) and [memory coordinator](/memory). The project was created as part of an Advanced Operating System assignment.

The project makes use of the [libvirt](https://libvirt.org/) library to manage the hypervisor and
the virtual machines.

## Scripts:

- `createvms.sh`: create the specified number of virtual machines:
```
./createvms.sh 8
```
will create and start 8 vms named `vm1`, `vm2`, ..., `vm8`.
- `destroyvms.sh`:
```
./destroyvms.sh 8
```
will destroy `vm1`, `vm2`, ..., `vm8`.
- `submission.sh`: generates the project submission zip

```
./submission.sh Firstname Lastname
```
will generate a `Firstname_Lastname_p1.zip` folder for submission.
