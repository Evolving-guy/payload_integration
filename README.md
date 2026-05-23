# Pixxel Embedded Linux Engineer Assignment
## Virtual Platform Device Driver + cFS User Applications

---
## Key Deliverables
pixxel_platform_driver.c\
Makefile\
pixxel_main.c\
pixxel_c.c

### 1. Platform Device Driver (`pixxel_driver/`)

A Linux kernel platform character device driver for the virtual device defined in the provided `qemuarm64.dtb`:  
pixxel-virt-dev@60000000 {  
compatible = "pixxel,virt-dev";  
reg = <0x00 0x60000000 0x00 0x1000>;  
};

**Key design decisions:**
- Matches device tree via `compatible = "pixxel,virt-dev"` string
- Maps register space from DTB using `platform_get_resource` + `devm_ioremap_resource`
- Exposes `/dev/pixxel` character device for user-space access
- Simulates Enable register (0x60000000) and Status register (0x60000004) using in-memory variables since the device is virtual with no real hardware backing


**To load the driver:**
```bash
sudo insmod pixxel_platform_driver.ko
lsmod
dmesg | tail -10
```

**To test the driver standalone:**
```bash
gcc -o test_app test_app.c
sudo ./test_app
```

---

### 2. cFS User-Space Applications (`cFS implementation/`)

Two applications built using NASA's Core Flight System (cFS) framework, communicating exclusively via the Software Bus (SB) service.  

#### pixxel_controller (`apps/pixxel_c/`)  
Acts as the hardware interface layer. Subscribes to command messages on `PIXXEL_CMD_MID`, opens `/dev/pixxel`, and executes hardware operations. Sends telemetry back to pixxel_main.  

Handles three command codes:  
- `PIXXEL_CMD_ENABLE_CC` — writes 1 to driver
- `PIXXEL_CMD_GET_STAT_CC` — reads status from driver, sends status telemetry
- `PIXXEL_CMD_NOOP_CC` — no operation

#### pixxel_main (`apps/pixxel_main/`)
Orchestrator application. Starts after pixxel_controller, executes the full verification sequence:

1. Sends ENABLE command to pixxel_controller via Software Bus
2. Waits 50ms (`OS_TaskDelay`) for hardware register propagation
3. Sends GET_STATUS command to controller
4. Receives status telemetry and verifies device is enabled (status == 1)
5. Logs SUCCESS or FAILURE via EVS

#### cFS Startup Script (`cfe_es_startup.scr`)  
CFE_APP, pixxel_c,    PIXXEL_CTRL_AppMain, PIXXEL_CTRL, 50, 16384, 0x0, 0;  
CFE_APP, pixxel_main, PIXXEL_MAIN_AppMain, PIXXEL_MAIN, 60, 16384, 0x0, 0;  
pixxel_controller loads at priority 50 before pixxel_main at priority 60, ensuring the hardware interface is ready before the orchestrator starts.

---

### 3. Yocto BSP Layer

Due to disk space constraints (6.3GB available, Yocto requires 50GB+), the full Yocto build could not be executed. 


---

## Build Instructions

### Driver
```bash
cd pixxel_driver/
make
sudo insmod pixxel_platform_driver.ko
```

### cFS Apps (native)
```bash
cd cFS/
make native_std.prep  
make native_std.install
cd build-native_std/exe/cpu1/
./core-cpu1
```

---

## Challenges

See `Pixxel_assignment README.pdf` for the full write-up.

*Assignment by Md Ashfaaq Khan*
