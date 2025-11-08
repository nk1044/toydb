# Database Storage Engine Project

This project contains three layers:

- `pflayer/`  → Page File (PF) Layer
- `amlayer/`  → B+ Tree Access Method (AM) Layer
- `spLayer/`  → Slotted-Page Heap File (HF) Layer

## How to Build and Run

### 1. Build PF Layer
```

cd pflayer
make

```

### 2. Build AM Layer
```

cd amlayer
make

```

### 3. Build and Run HF Layer Test
```

cd spLayer
make
./hf_test

```

### 4. Clean All Builds
```

cd pflayer && make clean
cd ../amlayer && make clean
cd ../spLayer && make clean

```
