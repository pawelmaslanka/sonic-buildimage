/*! \file main.c
 *
 * This example application demonstrates how the SDKLT can be
 * initialized using the application helper components supplied in the
 * $SDK/bcma directory.
 *
 * Upon initialization, the application will launch a command line
 * interface (CLI) from which the user can exercise the logical table
 * (LT) API for switch device management.
 */
/*
 * This license is set out in https://raw.githubusercontent.com/Broadcom-Network-Switching-Software/OpenBCM/master/Legal/LICENSE file.
 * 
 * Copyright 2007-2020 Broadcom Inc. All rights reserved.
 */

extern "C" {
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include <shr/shr_debug.h>
#include <shr/shr_pb.h>

#include <bcmdrd/bcmdrd_dev.h>
#include <bcmdrd/bcmdrd_feature.h>
#include <bcmbd/bcmbd.h>
#include <bcmmgmt/bcmmgmt.h>
#include <bcmmgmt/bcmmgmt_sysm_default.h>
#include <bcmlt/bcmlt.h>

#include <bcma/sys/bcma_sys_conf.h>
#include <bcma/cint/bcma_cintcmd.h>
#include <bcma/bcmpkt/bcma_bcmpktcmd.h>
#include <bcma/ha/bcma_ha.h>
#include <bcma/bsl/bcma_bslmgmt.h>
#include <bcma/bsl/bcma_bslcmd.h>

#include <shr/shr_pb.h>
#include <bcma/cli/bcma_clicmd_version.h>
// #include "version.h"
}

///////////////////////////////////////////

#include "lag/lag_service.hpp"

#include <spdlog/spdlog.h>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "lag.grpc.pb.h"

#include <sstream>

static char version_str[256];

void
version_init(void)
{
    shr_pb_t *pb;

    pb = shr_pb_create();
    shr_pb_printf(pb, "SDKLT Demo Application\n");
#ifdef VERSION_INFO
    shr_pb_printf(pb, "Release %s", VERSION_INFO);
#ifdef DATE_INFO
    shr_pb_printf(pb, " built on %s", DATE_INFO);
#endif
#ifdef SCM_INFO
    shr_pb_printf(pb, " (%s)", SCM_INFO);
#endif
    shr_pb_printf(pb, "\n");
#endif
    strncpy(version_str, shr_pb_str(pb), sizeof(version_str) - 1);
    shr_pb_destroy(pb);

    /* Update version string for CLI command 'version' */
    bcma_clicmd_version_string_set(version_str);
}

void
version_signon(void)
{
    printf("%s", version_str);
}

/*******************************************************************************
 * Local variables
 */

/* System configuration structure */
static bcma_sys_conf_t sys_conf, *isc;

/*******************************************************************************
 * Private CLI commands
 */

static int
drd_init(bcma_sys_conf_t *sc);

static int
clicmd_probe(bcma_cli_t *cli, bcma_cli_args_t *args)
{
    drd_init(isc);

    return BCMA_CLI_CMD_OK;
}

static bcma_cli_command_t shcmd_probe = {
    .name = "probe",
    .func = clicmd_probe,
    .desc = "Probe for devices.",
};

/*******************************************************************************
 * Private functions
 */

static int
cli_init(bcma_sys_conf_t *sc)
{
    /* Initialize CLI infracstructure with basic switch command set */
    bcma_sys_conf_cli_basic(sc);

    /* Add CLI commands for packet I/O driver */
    bcma_bcmpktcmd_add_cmds(sc->cli);

    /* Add CLI commands for BCMLT C interpreter (CINT) */
    bcma_cintcmd_add_cmds(sc->cli);

    /* Add local probe command to debug shell */
    bcma_cli_add_command(sc->dsh, &shcmd_probe, 0);

    /* Enable CLI redirection in BSL output hook */
    bcma_bslmgmt_redir_hook_set(bcma_sys_conf_cli_redir_bsl);

    /* Add CLI commands for controlling the system log */
    bcma_bslcmd_add_cmds(sc->cli);
    bcma_bslcmd_add_cmds(sc->dsh);

    return SHR_E_NONE;
}

static int
drd_init(bcma_sys_conf_t *sc)
{
    int ndev;
    int unit;
    shr_pb_t *pb;

    /* Probe/create devices */
    if ((ndev = bcma_sys_conf_drd_init(sc)) < 0) {
        return SHR_E_FAIL;
    }

    /* Log probe info */
    pb = shr_pb_create();
    shr_pb_printf(pb, "Found %d device%s.\n", ndev, (ndev == 1) ? "" : "s");
    for (unit = 0; unit < BCMDRD_CONFIG_MAX_UNITS; unit++) {
        if (bcmdrd_dev_exists(unit)) {
            shr_pb_printf(pb, "Unit %d: %s\n", unit, bcmdrd_dev_name(unit));
        }
    }
    LOG_INFO(BSL_LS_APPL_SHELL,
             (BSL_META("%s"), shr_pb_str(pb)));
    shr_pb_destroy(pb);

   /* Initialize CLI unit callback after probing devices */
    bcma_sys_conf_drd_cli_init(sc);

     return SHR_E_NONE;
}

static int
ha_init(bcma_sys_conf_t *sc, bool warm_boot, int ha_instance)
{
    int rv;
    int unit;

    rv = bcma_ha_mem_init(-1, 0, warm_boot, ha_instance);
    if (SHR_FAILURE(rv)) {
        printf ("Failed to create generic HA memory\n");
        return rv;
    }

    for (unit = 0; unit < BCMDRD_CONFIG_MAX_UNITS; unit++) {
        if (!bcmdrd_dev_exists(unit)) {
            continue;
        }
        rv = bcma_ha_mem_init(unit, 0, warm_boot, ha_instance);
        if (SHR_FAILURE(rv)) {
            printf("Failed to create HA memory for unit %d (%d)\n", unit, rv);
        }
        if (warm_boot) {
            char buf[80];
            rv = bcma_ha_mem_name_get(unit, sizeof(buf), buf);
            if (SHR_FAILURE(rv)) {
                printf("Failed to get HA file for unit %d (%d)\n", unit, rv);
            } else {
                printf("Warm boot - using HA file %s for unit %d\n",
                       buf, unit);
            }
        }
    }
    return SHR_E_NONE;
}

static int
ha_cleanup(bcma_sys_conf_t *sc, bool keep_ha_file)
{
    int rv;
    int unit;

    for (unit = 0; unit < BCMDRD_CONFIG_MAX_UNITS; unit++) {
        if (!bcmdrd_dev_exists(unit)) {
            continue;
        }
        if (keep_ha_file) {
            char buf[80];
            rv = bcma_ha_mem_name_get(unit, sizeof(buf), buf);
            if (SHR_FAILURE(rv)) {
                printf("Failed to get HA file for unit %d (%d)\n", unit, rv);
            } else {
                printf("Warm exit - keeping HA file %s for unit %d\n",
                       buf, unit);
            }
        }
        bcma_ha_mem_cleanup(unit, keep_ha_file);
    }

    bcma_ha_mem_cleanup(-1, keep_ha_file);

    return SHR_E_NONE;
}

static int
sdk_init(const char *config_file, bool warm_boot,
         bcmmgmt_issu_info_t *issu_info, bool debug_shell,
         int ha_instance)
{
    int rv;

    /* Initialize system configuration structure */
    if (!isc) {
        isc = &sys_conf;
        bcma_sys_conf_init(isc);
    }

    /* Initialize system log output */
    rv = bcma_bslmgmt_init();
    if (SHR_FAILURE(rv)) {
        printf("bcma_bslmgmt_init failed (%s)\n", shr_errmsg(rv));
        return rv;
    }

    /* Initialize CLI */
    rv = cli_init(isc);
    if (SHR_FAILURE(rv)) {
        printf("cli_init failed (%s)\n", shr_errmsg(rv));
        return rv;
    }

    /* Initialize test component */
    if (bcma_sys_conf_test_init(isc) < 0) {
        return SHR_E_FAIL;
    }

    if (debug_shell) {
        /* Debug shell main loop */
        bcma_cli_cmd_loop(isc->dsh);
    }

    /* Initialize DRD */
    rv = drd_init(isc);
    if (SHR_FAILURE(rv)) {
        printf("drd_init failed (%s)\n", shr_errmsg(rv));
        return rv;
    }

    /* Initialize HA */
    rv = ha_init(isc, warm_boot, ha_instance);
    if (SHR_FAILURE(rv)) {
        printf("ha_init failed (%s)\n", shr_errmsg(rv));
        return rv;
    }

    /* ISSU upgrade start */
    rv = bcmmgmt_issu_start(warm_boot, issu_info);
    if (SHR_FAILURE(rv)) {
        printf("bcmmgmt_issu_start failed (%s)\n", shr_errmsg(rv));
        return rv;
    }

    /* Default init sequence */
    rv = bcmmgmt_init(warm_boot, warm_boot ? NULL : config_file);
    if (SHR_FAILURE(rv)) {
        printf("bcmmgmt_init failed (%s)\n", shr_errmsg(rv));
        bcmmgmt_issu_done();
        return rv;
    }

    /* ISSU upgrade done */
    rv = bcmmgmt_issu_done();
    if (SHR_FAILURE(rv)) {
        printf("bcmmgmt_issu_done failed (%s)\n", shr_errmsg(rv));
        return rv;
    }

    /* Auto-run CLI script */
    bcma_sys_conf_rcload_run(isc);

    return 0;
}

// static int
// sdk_run(void)
// {
//     /* Check for valid sys_conf structure */
//     if (isc == NULL) {
//         return -1;
//     }

//     /* CLI main loop */
//     bcma_cli_cmd_loop(isc->cli);

//     return 0;
// }

static int
sdk_cleanup(bool keep_ha_file)
{
    int rv;

    /* Check for valid sys_conf structure */
    if (isc == NULL) {
        return -1;
    }

    /* Shut down SDK */
    rv = bcmmgmt_shutdown(true);
    if (SHR_FAILURE(rv)) {
        printf("bcmmgmt_shutdown failed (%s)\n", shr_errmsg(rv));
        return rv;
    }

    /* Clean up HA file */
    rv = ha_cleanup(isc, keep_ha_file);
    if (SHR_FAILURE(rv)) {
        printf("ha_cleanup failed (%s)\n", shr_errmsg(rv));
        return rv;
    }

    /* Remove devices from DRD */
    bcma_sys_conf_drd_cleanup(isc);

    /* Reset unit numbers in CLI */
    bcma_sys_conf_drd_cli_cleanup(isc);

    /* Cleanup test component */
    bcma_sys_conf_test_cleanup(isc);

    /* Clean up CLI */
    bcma_sys_conf_cli_cleanup(isc);

    /* Clean up system log */
    rv = bcma_bslmgmt_cleanup();
    if (SHR_FAILURE(rv)) {
        printf("bcma_bslmgmt_cleanup failed (%s)\n", shr_errmsg(rv));
        return rv;
    }

    /* Release system configuration structure */
    bcma_sys_conf_cleanup(isc);

    return 0;
}

/*******************************************************************************
 * Public functions
 */

static bool keep_ha_file = false;

int init_asic(int argc, char *argv[]) {
        const char *config_file = "config.yml";
    bcmmgmt_issu_info_t issu_info, *issu_info_p = NULL;
    bool debug_shell = false;
    bool warm_boot = false;
    int ha_instance = -1;

    issu_info.start_ver = NULL;
    issu_info.current_ver = NULL;

    while (1) {
        int ch;

        ch = getopt(argc, argv, "dhkl:s:uv:wy:g:");
        if (ch == -1) {
            /* No more options */
            break;
        }

        switch (ch) {
        case 'd':
            debug_shell = true;
            break;

        case 'g':
            ha_instance = sal_atoi(optarg);
            break;
        case 'h':
            printf("Usage:\n");
            printf("%s [options]\n"
                   "-d          Enter debug shell before starting SDK.\n"
                   "-g <inst>   Specify HA file specific instance.\n"
                   "-h          Show this help text.\n"
                   "-k          Keep high-availability file.\n"
                   "-w          Run in warm-boot mode.\n"
                   "-u          Enable version upgrade in warm-boot mode.\n"
                   "-s <ver>    Specify start version for upgrade.\n"
                   "-v <ver>    Specify target version for upgrade.\n"
                   "-y <file>   Specify YAML configuration file.\n"
                   "-l <file>   Specify CLI auto-run script file.\n"
                   "\n", argv[0]);
            exit(-2);
            break;

        case 'k':
            keep_ha_file = true;
            break;

        case 'w':
            warm_boot = true;
            break;

        case 'y':
            config_file = optarg;
            break;

        case 'u':
            issu_info_p = &issu_info;
            break;

        case 's':
            issu_info.start_ver = optarg;
            break;

        case 'v':
            issu_info.current_ver = optarg;
            break;

        case 'l':
            bcma_sys_conf_rcload_set(optarg);
            break;

        default:
            exit(-2);
            break;
        }
    }

    /* Extra arguments may indicate a syntax error as well */
    if (optind < argc) {
        printf("Unexpected argument: %s\n", argv[optind]);
        exit(-2);
    }

    /* Initialize and display version string */
    version_init();
    version_signon();

    /* Initialize SDK */
    sdk_init(config_file, warm_boot, issu_info_p, debug_shell, ha_instance);

    /* CLI main loop */
    // sdk_run();

    /* Clean up SDK */
    // sdk_cleanup(keep_ha_file);

    return 0;
}

void RunServer(uint16_t port) {
    std::ostringstream stringStream;
    stringStream << "127.0.0.1:";
    stringStream << port;
    std::string server_address = stringStream.str();
    LagService lagService;

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    grpc::ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&lagService);
    // Finally assemble the server.
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    spdlog::info("Server listening on {}", server_address);

    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    server->Wait();
}

int
main(int argc, char *argv[])
{
    if (init_asic(argc, argv)) {
        exit(EXIT_FAILURE);
    }

    RunServer(50052);
    /* Clean up SDK */
    sdk_cleanup(keep_ha_file);

    exit(0);
}


// #include <cassert>
// #include <cstdlib>
// #include <cstdio>

// #include <iostream>

// #define BCM_LTSW_SUPPORT 1

// namespace hsdk {
//     bool init(int unit);
//     void shutdown(int unit);
// }

// int main(const int argc, const char* argv[]) {
//     assert(hsdk::init(0));
//     ::exit(EXIT_SUCCESS);
// }

// #ifndef GTEST_UT
// // #include "init.hpp"
// // #include "lcmgr_module.h"

// extern "C" {
// #include <unistd.h>
// #include <stdlib.h>

// #include <sal/core/boot.h>
// #include <sal/appl/sal.h>
// #include <sal/appl/config.h>
// #include <sal/appl/pci.h>
// #include <soc/debug.h>
// #include <soc/drv.h>
// #include <shared/shr_bprof.h>

// #include <appl/diag/system.h>

// #include <linux-bde.h>

// #ifdef BCM_LTSW_SUPPORT
// #include <appl/diag/sysconf_ltsw.h>
// #endif

// /* The bus properties are (currently) the only system specific
//  * settings required. 
//  * These must be defined beforehand 
//  */

// #define SYS_BE_PIO 0
// #define SYS_BE_PACKET 0
// #define SYS_BE_OTHER 0

// #ifndef SYS_BE_PIO
// #error "SYS_BE_PIO must be defined for the target platform"
// #endif
// #ifndef SYS_BE_PACKET
// #error "SYS_BE_PACKET must be defined for the target platform"
// #endif
// #ifndef SYS_BE_OTHER
// #error "SYS_BE_OTHER must be defined for the target platform"
// #endif

// #if !defined(SYS_BE_PIO) || !defined(SYS_BE_PACKET) || !defined(SYS_BE_OTHER)
// #error "platform bus properties not defined."
// #endif

// #include <bcm/init.h>
// } 

// #endif // GTEST_UT

// using namespace hsdk;

// #define IF_BCM_FAILED_RETURN_FALSE(rv, errmsg)      \
//     do {                                            \
//         if (rv < 0) {                               \
//             std::cerr << "sal_core_init() failed\n";   \
//             return false;                           \
//         }                                           \
//     } while (0)

// /* Function defined in linux-user-bde.c */
// extern "C" int
// bde_icid_get(int d, uint8 *data, int len);

// static soc_chip_info_vectors_t chip_info_vect = {
//     bde_icid_get
// };

// static void
// chip_info_vect_config(void)
// {
//     soc_chip_info_vect_config(&chip_info_vect);
// }

// #ifdef INCLUDE_KNET

// extern "C" {
// #include <soc/knet.h>
// #include <bcm-knet-kcom.h>

// /* Function defined in linux-user-bde.c */
// extern int
// bde_irq_mask_set(int unit, uint32 addr, uint32 mask);
// extern int
// bde_hw_unit_get(int unit, int inverse);
// }

// static soc_knet_vectors_t knet_vect_bcm_knet = {
//     {
//         bcm_knet_kcom_open,
//         bcm_knet_kcom_close,
//         bcm_knet_kcom_msg_send,
//         bcm_knet_kcom_msg_recv
//     },
//     bde_irq_mask_set,
//     bde_hw_unit_get
// };

// static void
// knet_kcom_config(void)
// {
//     /* Direct IOCTL by default */
//     soc_knet_config(&knet_vect_bcm_knet);
//     char bcm_knet[sal_strlen("bcm-knet") + 1] = {};
//     sal_strcpy(bcm_knet, "bcm-knet");
//     var_set("kcom", bcm_knet, 0, 0);
// }

// #endif /* INCLUDE_KNET */

// #ifdef BCM_INSTANCE_SUPPORT
// static int
// _instance_config(const char *inst)
// {
//     const char *ptr;
// #ifndef NO_SAL_APPL
//     char *estr;
// #endif
//     linux_bde_device_bitmap_t dev_mask;
//     unsigned int  dma_size;

//     if (inst == NULL) {
// #ifndef NO_SAL_APPL
//         estr = "./bcm.user -i <dev_mask>[:dma_size_in_mb] \n";
//         sal_console_write(estr, sal_strlen(estr) + 1);
// #endif
//         return -1;
//     }
//     *dev_mask = strtol(inst, NULL, 0);
//     if ((ptr = strchr(inst,':')) == NULL) {
//         dma_size = 4;
//     } else {
//         ptr++;
//         dma_size = strtol(ptr, NULL, 0);
//     }

//     if (dma_size < 4) {
// #ifndef NO_SAL_APPL
//         estr = "dmasize must be > 4M and a power of 2 (4M, 8M etc.)\n";
//         sal_console_write(estr, sal_strlen(estr) + 1);
// #endif
//         return -1;
//     } else {
//         if ( (dma_size >> 2) & ((dma_size >> 2 )-1)) {
// #ifndef NO_SAL_APPL
//             estr = "dmasize must be a power of 2 (4M, 8M etc.)\n";
//             sal_console_write(estr, sal_strlen(estr) + 1);
// #endif
//             return -1;
//         }
//     }
    
//     return linux_bde_instance_config(dev_mask, dma_size);
// }
// #endif


// void startup() {
//     int argc = 0;
//     char* argv[] = {};
//     int i, len;
//     char *envstr;
//     char *config_file, *config_temp;
// #ifdef BCM_INSTANCE_SUPPORT
//     const char *inst = NULL;
// #endif
// #ifdef BCM_LTSW_SUPPORT
//     int cfg_file_idx = 0;
// #endif

//     if ((envstr = getenv("BCM_CONFIG_FILE")) != NULL) {
//         config_file = envstr;
//         len = sal_strlen(config_file);
//         if ((config_temp = (char *)sal_alloc(len+5, NULL)) != NULL) {
//             sal_strcpy(config_temp, config_file);
//             sal_strcpy(&config_temp[len], ".tmp");
// #ifndef NO_SAL_APPL
//             sal_config_file_set(config_file, config_temp);
// #endif
//             sal_free(config_temp);
//         }
//     }

// // Destiny
//     char yaml_conf_file[] = "/usr/bin/bcm56880_a0-generic-32x400.config.yml";
//     if (sysconf_ltsw_config_file_set(cfg_file_idx, yaml_conf_file) < 0) {
//         printf("Invalid YAML configuration file: %s\n", yaml_conf_file);
//         exit(1);
//     }
// // Destiny

// #ifdef BCM_LTSW_SUPPORT
//     for (i = 1; i < argc; i++) {
//         if (strcmp(argv[i], "-y") == 0) {
//             if (++i >= argc) {
//                 printf("No YAML configuration file specified\n");
//                 exit(1);
//             }
//             if (sysconf_ltsw_config_file_set(cfg_file_idx, argv[i]) < 0) {
//                 printf("Invalid YAML configuration file: %s\n", argv[i]);
//                 exit(1);
//             }
//             cfg_file_idx++;
//         }
//     }
// #endif

// #ifdef BCM_INSTANCE_SUPPORT
//     for (i = 1; i < argc; i++) {
//          if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--instance")) {
//             inst = argv[i+1];            
//             /*
//              * specify the dev_mask and its dma_size (optional,default:4MB)
//              * bcm.user -i 0x1[:8]
//              */
//             if (_instance_config(inst) < 0){
// #ifndef NO_SAL_APPL
//                 char *estr = "config error!\n";
//                 sal_console_write(estr, sal_strlen(estr) + 1);
// #endif
//                 exit(1);
//             }
//         }
//     }
// #endif

//     if (sal_core_init() < 0
// #ifndef NO_SAL_APPL
//         || sal_appl_init() < 0
// #endif
//         ) {
//         /*
//          * If SAL initialization fails then printf will most
//          * likely assert or fail. Try direct console access
//          * instead to get the error message out.
//          */
// #ifndef NO_SAL_APPL
//         char estr[] = "SAL Initialization failed\r\n";
//         sal_console_write(estr, sal_strlen(estr) + 1);
// #endif
//         exit(1);
//     }

//     for (i = 1; i < argc; i++) {
//         if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "--reload")) {
//             sal_boot_flags_set(sal_boot_flags_get() | BOOT_F_RELOAD);
// #if defined (BCM_DNX_SUPPORT) || defined (BCM_DNXF_SUPPORT)
//         } else if (!strcmp(argv[i], "--fast")) {
//             soc_verify_fast_init_set(TRUE);
//         } else if (!strcmp(argv[i], "--verify")) {
//             soc_verify_fast_init_set(FALSE);
// #endif /* defined (BCM_DNX_SUPPORT) || defined (BCM_DNXF_SUPPORT) */
// #ifdef BCM_LTSW_SUPPORT
//         } else if (!strcmp(argv[i], "-a")) {
//             if (++i >= argc) {
//                 printf("No path specified for storing HA state file.\n");
//                 exit(1);
//             }
//             if (sysconf_ltsw_ha_check_set(1, argv[i]) < 0) {
//                 printf("Can not set SDK HA check status.\n");
//                 exit(1);
//             }
//         } else if (!strcmp(argv[i], "-s")) {
//             if (++i >= argc) {
//                 printf("No SDK ISSU start version specified\n");
//                 exit(1);
//             }
//             if (sysconf_ltsw_ha_start_version_set(argv[i]) < 0) {
//                 printf("Can not set SDK ISSU start version\n");
//                 exit(1);
//             }
//         } else if (!strcmp(argv[i], "-v")) {
//             if (++i >= argc) {
//                 printf("No SDK ISSU current version specified\n");
//                 exit(1);
//             }
//             if (sysconf_ltsw_ha_current_version_set(argv[i]) < 0) {
//                 printf("Can not set SDK ISSU current version\n");
//                 exit(1);
//             }
//         } else if (!strcmp(argv[i], "-g")) {
//             if (++i >= argc) {
//                 printf("No ID number for high-availability files specified\n");
//                 exit(1);
//             }
//             if (sysconf_ltsw_ha_no_del_set(sal_atoi(argv[i])) < 0) {
//                 printf("Can not specify ID number for high-availability files\n");
//                 exit(1);
//             }
// #endif
//         }
//     }

// #ifdef INCLUDE_KNET
//     knet_kcom_config();
// #endif
//     chip_info_vect_config();
// }

// bool hsdk::init(int unit) {
//     startup();
//     diag_sdk_init();

//     return true;
// }

// void hsdk::shutdown(int unit) {
//     // linux_bde_destroy(bde);
//     bcm_detach(unit);
// }
