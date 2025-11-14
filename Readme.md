# Database Storage Manager Project

(tags: c, dbms, storage-engine, slotted-pages, b-plus-tree)

## Overview

This project implements a three-layer database storage engine, consisting of:

1. **PF Layer (Paged File Layer)** — Handles pages, buffer pool, LRU/MRU replacement policies.
2. **HF Layer (Heap File Layer)** — Provides variable-length record storage using slotted pages.
3. **AM Layer (Access Method Layer)** — Implements B+-Tree indexing over PF files.

Performance tests evaluate each layer separately using custom test workloads.

---

## Directory Structure

```
.
├── pfLayer/      # PF layer implementation (buffer manager, page file, LRU/MRU)
├── hfLayer/      # Slotted-page heap file implementation
├── amLayer/      # B+-Tree index implementation
├── tests/        # Test programs for PF, HF, AM layers
└── data/         # Dataset files (courses.txt)
```

---

## Objectives

### PF Layer

* Implement an in-memory buffer pool with configurable size.
* Support LRU and MRU replacement policies.
* Track logical and physical I/O counts.
* Measure performance under different read/write mixes.

### HF Layer

* Implement slotted-page record storage for variable-length records.
* Support insertion, deletion, and sequential scan.
* Compare storage utilization vs. static fixed-record layout.

### AM Layer

* Implement B+-Tree index creation and lookup.
* Compare different index build strategies:

  * Build-from-existing (unsorted)
  * Incremental random inserts
  * Bulk-load sorted keys

---

# Setup and Build Guide

## 1. Build PF Layer

```
cd pfLayer
make
```

## 2. Build HF Layer

```
cd hfLayer
make
```

## 3. Build AM Layer

```
cd amLayer
make
```

## 4. Build Tests

```
cd tests
make
```

---

# Running the Tests

## PF Layer Test (LRU/MRU, read/write mixes)

```
make test1
./test1
```

## HF Layer Test (Variable vs Static Storage)

```
make test2
./test2
```

## AM Layer Test (Index construction)

```
make test3
./test3
```

---

# Diagrams and Experimental Results

Below are all required diagrams and graphs, inserted as markdown image links.
Replace `figures/...` with actual paths in your repo.

---

## Slotted Page Diagram

![WhatsApp Image 2025-11-14 at 10 20 02](https://github.com/user-attachments/assets/eafac76f-40d8-4773-8314-84c40bde3de2)

---

## PF Layer: LRU vs MRU Performance

![WhatsApp Image 2025-11-14 at 10 17 06](https://github.com/user-attachments/assets/a6f88c71-a1fd-4ed2-8b2b-1d7221f9aa3a)

---

## Static Storage vs Variable Storage (2 Diagrams)

### Pages Required vs Record Size

![WhatsApp Image 2025-11-14 at 10 16 59](https://github.com/user-attachments/assets/99c0c86e-c91a-47e8-9412-6dfae94dddb2)

### Utilization vs Record Size

![WhatsApp Image 2025-11-14 at 10 16 52](https://github.com/user-attachments/assets/dd5910d5-0397-4fcb-aee0-9ad465a850bf)

---

## Index Construction

### Build Time

![WhatsApp Image 2025-11-14 at 10 25 36 (1)](https://github.com/user-attachments/assets/1eda78c3-3a97-4bbd-b997-12f85d49546b)

### Pages Used

![WhatsApp Image 2025-11-14 at 10 25 36](https://github.com/user-attachments/assets/034727c6-189c-4f82-bde4-ca323d08c442)

### Lookup Time

![WhatsApp Image 2025-11-14 at 10 25 37](https://github.com/user-attachments/assets/4f8199df-da30-4761-b826-81f20812e63b)

---

# Test Summary

### test1 – PF Layer Performance

* Creates 2000 pages.
* Runs multiple read/write mixes.
* Compares LRU vs MRU.
* Outputs timing and I/O statistics.

### test2 – HF Layer Space Utilization

* Builds a variable-length heap file using slotted pages.
* Builds several static fixed-record files with different record sizes.
* Computes pages, stored bytes, and utilization.
* Provides an experimental comparison table.

### test3 – AM Layer Index Comparison

* Extracts all keys from HF file.
* Builds B+-Tree index using three strategies:

  1. Build-from-existing
  2. Incremental random inserts
  3. Bulk-load sorted
* Measures build time, number of pages created, and lookup latency.

---

# Notes

* All components rely on proper initialization using `PF_Init()`.
* All tests require `courses.txt` in the `data/` directory.
* Remove old files before re-running tests for accurate results.
