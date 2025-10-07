# BRACU_CSE321_Operating_Systems_Summer_25

# 🧠 Operating Systems Lab Project — File System Simulation

## 📘 Overview
This project simulates a simple file system using C. It consists of two main programs — **mkfs_builder** and **mkfs_adder** — that demonstrate how a file system can be initialized, managed, and updated similar to low-level OS operations.

---

## ⚙️ Components

### 🏗️ mkfs_builder
- Initializes the file system.
- Creates the superblock, inodes, and data blocks.
- Defines total blocks, inode size, and free space.

### ➕ mkfs_adder
- Adds files into the system.
- Allocates inodes and data blocks.
- Updates metadata and directory structure.

---

## 🧩 Features
✅ Superblock + inode structure  
✅ Block allocation management  
✅ File metadata tracking  
✅ Disk image simulation (`t1.img`, `t2.img`)  
✅ Sample input files (`file_9.txt`, `file_13.txt`, etc.)  

---

## 💻 How to Run
```bash
gcc mkfs_builder.c -o mkfs_builder
gcc mkfs_adder.c -o mkfs_adder
./mkfs_builder
./mkfs_adder
