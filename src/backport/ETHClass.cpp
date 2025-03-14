/*
 ETH.h - espre ETH PHY support.
 Based on WiFi.h from Arduino WiFi shield library.
 Copyright (c) 2011-2014 Arduino.  All right reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <Arduino.h>
#if defined(ESPCONNECT_ETH_SUPPORT) && ESP_IDF_VERSION_MAJOR < 5 && defined(ETH_PHY_SPI_SCK) && defined(ETH_PHY_SPI_MISO) && defined(ETH_PHY_SPI_MOSI) && defined(ETH_PHY_CS) && defined(ETH_PHY_IRQ) && defined(ETH_PHY_RST)

#include "ETHClass.h"
#if ESP_IDF_VERSION_MAJOR > 3
#include "esp_eth.h"
#include "esp_eth_com.h"
#include "esp_eth_mac.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#if CONFIG_IDF_TARGET_ESP32
#include "soc/emac_ext_struct.h"
#include "soc/rtc.h"
// #include "soc/io_mux_reg.h"
// #include "hal/gpio_hal.h"
#endif
#else
#include "eth_phy/phy.h"
#include "eth_phy/phy_lan8720.h"
#include "eth_phy/phy_tlk110.h"
#endif
#include "lwip/dns.h"
#include "lwip/err.h"

extern void tcpipInit();
extern void add_esp_interface_netif(esp_interface_t interface, esp_netif_t* esp_netif); /* from WiFiGeneric */

#if ESP_IDF_VERSION_MAJOR > 3

/**
 * @brief Callback function invoked when lowlevel initialization is finished
 *
 * @param[in] eth_handle: handle of Ethernet driver
 *
 * @return
 *       - ESP_OK: process extra lowlevel initialization successfully
 *       - ESP_FAIL: error occurred when processing extra lowlevel initialization
 */

static eth_clock_mode_t eth_clock_mode = ETH_CLK_MODE;

#if CONFIG_ETH_RMII_CLK_INPUT
/*
static void emac_config_apll_clock(void)
{
    // apll_freq = xtal_freq * (4 + sdm2 + sdm1/256 + sdm0/65536)/((o_div + 2) * 2)
    rtc_xtal_freq_t rtc_xtal_freq = rtc_clk_xtal_freq_get();
    switch (rtc_xtal_freq) {
    case RTC_XTAL_FREQ_40M: // Recommended
        // 50 MHz = 40MHz * (4 + 6) / (2 * (2 + 2) = 50.000
        // sdm0 = 0, sdm1 = 0, sdm2 = 6, o_div = 2
        rtc_clk_apll_enable(true, 0, 0, 6, 2);
        break;
    case RTC_XTAL_FREQ_26M:
        // 50 MHz = 26MHz * (4 + 15 + 118 / 256 + 39/65536) / ((3 + 2) * 2) = 49.999992
        // sdm0 = 39, sdm1 = 118, sdm2 = 15, o_div = 3
        rtc_clk_apll_enable(true, 39, 118, 15, 3);
        break;
    case RTC_XTAL_FREQ_24M:
        // 50 MHz = 24MHz * (4 + 12 + 255 / 256 + 255/65536) / ((2 + 2) * 2) = 49.499977
        // sdm0 = 255, sdm1 = 255, sdm2 = 12, o_div = 2
        rtc_clk_apll_enable(true, 255, 255, 12, 2);
        break;
    default: // Assume we have a 40M xtal
        rtc_clk_apll_enable(true, 0, 0, 6, 2);
        break;
    }
}
*/
#endif

/*
static esp_err_t on_lowlevel_init_done(esp_eth_handle_t eth_handle){
#if CONFIG_IDF_TARGET_ESP32
    if(eth_clock_mode > ETH_CLOCK_GPIO17_OUT){
        return ESP_FAIL;
    }
    // First deinit current config if different
#if CONFIG_ETH_RMII_CLK_INPUT
    if(eth_clock_mode != ETH_CLOCK_GPIO0_IN && eth_clock_mode != ETH_CLOCK_GPIO0_OUT){
        pinMode(0, INPUT);
    }
#endif

#if CONFIG_ETH_RMII_CLK_OUTPUT
#if CONFIG_ETH_RMII_CLK_OUTPUT_GPIO0
    if(eth_clock_mode > ETH_CLOCK_GPIO0_OUT){
        pinMode(0, INPUT);
    }
#elif CONFIG_ETH_RMII_CLK_OUT_GPIO == 16
    if(eth_clock_mode != ETH_CLOCK_GPIO16_OUT){
        pinMode(16, INPUT);
    }
#elif CONFIG_ETH_RMII_CLK_OUT_GPIO == 17
    if(eth_clock_mode != ETH_CLOCK_GPIO17_OUT){
        pinMode(17, INPUT);
    }
#endif
#endif

    // Setup interface for the correct pin
#if CONFIG_ETH_PHY_INTERFACE_MII
    EMAC_EXT.ex_phyinf_conf.phy_intf_sel = 4;
#endif

    if(eth_clock_mode == ETH_CLOCK_GPIO0_IN){
#ifndef CONFIG_ETH_RMII_CLK_INPUT
        // RMII clock (50MHz) input to GPIO0
        //gpio_hal_iomux_func_sel(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_EMAC_TX_CLK);
        //PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[0]);
        pinMode(0, INPUT);
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[0], 5);
        EMAC_EXT.ex_clk_ctrl.ext_en = 1;
        EMAC_EXT.ex_clk_ctrl.int_en = 0;
        EMAC_EXT.ex_oscclk_conf.clk_sel = 1;
#endif
    } else {
        if(eth_clock_mode == ETH_CLOCK_GPIO0_OUT){
#ifndef CONFIG_ETH_RMII_CLK_OUTPUT_GPIO0
            // APLL clock output to GPIO0 (must be configured to 50MHz!)
            //gpio_hal_iomux_func_sel(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
            //PIN_INPUT_DISABLE(GPIO_PIN_MUX_REG[0]);
            pinMode(0, OUTPUT);
            PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[0], 1);
            // Choose the APLL clock to output on GPIO
            REG_WRITE(PIN_CTRL, 6);
#endif
        } else if(eth_clock_mode == ETH_CLOCK_GPIO16_OUT){
#if CONFIG_ETH_RMII_CLK_OUT_GPIO != 16
            // RMII CLK (50MHz) output to GPIO16
            //gpio_hal_iomux_func_sel(PERIPHS_IO_MUX_GPIO16_U, FUNC_GPIO16_EMAC_CLK_OUT);
            //PIN_INPUT_DISABLE(GPIO_PIN_MUX_REG[16]);
            pinMode(16, OUTPUT);
            PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[16], 5);
#endif
        } else if(eth_clock_mode == ETH_CLOCK_GPIO17_OUT){
#if CONFIG_ETH_RMII_CLK_OUT_GPIO != 17
            // RMII CLK (50MHz) output to GPIO17
            //gpio_hal_iomux_func_sel(PERIPHS_IO_MUX_GPIO17_U, FUNC_GPIO17_EMAC_CLK_OUT_180);
            //PIN_INPUT_DISABLE(GPIO_PIN_MUX_REG[17]);
            pinMode(17, OUTPUT);
            PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[17], 5);
#endif
        }
#if CONFIG_ETH_RMII_CLK_INPUT
        EMAC_EXT.ex_clk_ctrl.ext_en = 0;
        EMAC_EXT.ex_clk_ctrl.int_en = 1;
        EMAC_EXT.ex_oscclk_conf.clk_sel = 0;
        emac_config_apll_clock();
        EMAC_EXT.ex_clkout_conf.div_num = 0;
        EMAC_EXT.ex_clkout_conf.h_div_num = 0;
#endif
    }
#endif
    return ESP_OK;
}
*/

/**
 * @brief Callback function invoked when lowlevel deinitialization is finished
 *
 * @param[in] eth_handle: handle of Ethernet driver
 *
 * @return
 *       - ESP_OK: process extra lowlevel deinitialization successfully
 *       - ESP_FAIL: error occurred when processing extra lowlevel deinitialization
 */
// static esp_err_t on_lowlevel_deinit_done(esp_eth_handle_t eth_handle){
//     return ESP_OK;
// }

#else
static int _eth_phy_mdc_pin = -1;
static int _eth_phy_mdio_pin = -1;
static int _eth_phy_power_pin = -1;
static eth_phy_power_enable_func _eth_phy_power_enable_orig = NULL;

static void _eth_phy_config_gpio(void) {
  if (_eth_phy_mdc_pin < 0 || _eth_phy_mdio_pin < 0) {
    log_e("MDC and MDIO pins are not configured!");
    return;
  }
  phy_rmii_configure_data_interface_pins();
  phy_rmii_smi_configure_pins(_eth_phy_mdc_pin, _eth_phy_mdio_pin);
}

static void _eth_phy_power_enable(bool enable) {
  pinMode(_eth_phy_power_pin, OUTPUT);
  digitalWrite(_eth_phy_power_pin, enable);
  delay(1);
}
#endif

ETHClass::ETHClass()
    : initialized(false), staticIP(false)
#if ESP_IDF_VERSION_MAJOR > 3
      ,
      eth_handle(NULL), eth_netif(NULL)
#endif
      ,
      started(false) {
}

ETHClass::~ETHClass() {}

bool ETHClass::beginSPI(int miso, int mosi, int sck, int cs, int rst, int irq,
                        spi_host_device_t host_id,
                        uint8_t phy_addr,
                        int clk_mhz,
                        bool use_mac_from_efuse) {
#if ESP_IDF_VERSION_MAJOR > 3
  esp_err_t err;
  tcpipInit();
  tcpip_adapter_set_default_eth_handlers();

  if (rst > -1) {
    pinMode(rst, OUTPUT);
    digitalWrite(rst, HIGH);
    delay(250);
    digitalWrite(rst, LOW);
    delay(50);
    digitalWrite(rst, HIGH);
    delay(350);
  }

  if (use_mac_from_efuse) {
    uint8_t p[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    esp_efuse_mac_get_custom(p);
    esp_base_mac_addr_set(p);
  }

  esp_eth_mac_t* eth_mac = NULL;
  esp_eth_phy_t* eth_phy = NULL;

  // Create instance(s) of esp-netif for SPI Ethernet(s)
  esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();

  esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
  cfg.base = &esp_netif_config;
  cfg.stack = ESP_NETIF_NETSTACK_DEFAULT_ETH;

  char if_key_str[10] = "ETH_SPI_0";
  char if_desc_str[10] = "eth0";
  esp_netif_config.if_key = if_key_str;
  esp_netif_config.if_desc = if_desc_str;
  esp_netif_config.route_prio = 30;
  eth_netif = esp_netif_new(&cfg);

  // Init MAC and PHY configs to default
  eth_mac_config_t mac_config_spi = ETH_MAC_DEFAULT_CONFIG();
  eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

  // Install GPIO ISR handler to be able to service SPI Eth modlues interrupts
  gpio_install_isr_service(0);

  // Init SPI bus
  spi_device_handle_t spi_handle = {0};
  spi_bus_config_t buscfg = {
      .mosi_io_num = mosi,
      .miso_io_num = miso,
      .sclk_io_num = sck,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .data4_io_num = 0,
      .data5_io_num = 0,
      .data6_io_num = 0,
      .data7_io_num = 0,
      .max_transfer_sz = 0,
      .flags = 0,
      .intr_flags = 0
  };
  err = spi_bus_initialize(host_id, &buscfg, SPI_DMA_CH_AUTO);
  if (err != ESP_OK) {
    log_e("spi_bus_initialize failed");
    return false;
  }

  spi_device_interface_config_t devcfg = {
    .command_bits = 16, // Actually it's the address phase in W5500 SPI frame
    .address_bits = 8,  // Actually it's the control phase in W5500 SPI frame
    .dummy_bits = 0,
    .mode = 0,
    .duty_cycle_pos = 0,
    .cs_ena_pretrans = 0,
    .cs_ena_posttrans = 0,
    .clock_speed_hz = clk_mhz * 1000 * 1000,
    .input_delay_ns = 0,
    .spics_io_num = 0,
    .flags = 0,
    .queue_size = 20,
    .pre_cb = nullptr,
    .post_cb = nullptr
  };

  // Set SPI module Chip Select GPIO
  devcfg.spics_io_num = cs;

  err = spi_bus_add_device(host_id, &devcfg, &spi_handle);
  if (err != ESP_OK) {
    log_e("spi_bus_add_device failed");
    return false;
  }
  // w5500 ethernet driver is based on spi driver
  eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(spi_handle);

  // Set remaining GPIO numbers and configuration used by the SPI module
  w5500_config.int_gpio_num = irq;
  phy_config.phy_addr = phy_addr;
  phy_config.reset_gpio_num = rst;

  eth_mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config_spi);
  if (eth_mac == NULL) {
    log_e("esp_eth_mac_new_esp32 failed");
    return false;
  }

  eth_phy = esp_eth_phy_new_w5500(&phy_config);
  if (eth_phy == NULL) {
    log_e("esp_eth_phy_new failed");
    return false;
  }

  eth_handle = NULL;
  esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(eth_mac, eth_phy);
  if (esp_eth_driver_install(&eth_config, &eth_handle) != ESP_OK || eth_handle == NULL) {
    log_e("esp_eth_driver_install failed");
    return false;
  }

  uint8_t address[6] = {0, 0, 0, 0, 0, 0};
  if (esp_read_mac(address, ESP_MAC_ETH) != ESP_OK) {
    log_e("esp_read_mac failed");
    return false;
  }

  esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, address);

  /* attach Ethernet driver to TCP/IP stack */
  if (esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)) != ESP_OK) {
    log_e("esp_netif_attach failed");
    return false;
  }

  /* attach to WiFiGeneric to receive events */
  add_esp_interface_netif(ESP_IF_ETH, eth_netif);

  if (esp_eth_start(eth_handle) != ESP_OK) {
    log_e("esp_eth_start failed");
    return false;
  }
#endif
  // holds a few milliseconds to let DHCP start and enter into a good state
  // FIX ME -- adresses issue https://github.com/espressif/arduino-esp32/issues/5733
  delay(50);

  return true;
}

bool ETHClass::begin(uint8_t phy_addr, int power, int mdc, int mdio, eth_phy_type_t type, eth_clock_mode_t clock_mode, bool use_mac_from_efuse) {
#if ESP_IDF_VERSION_MAJOR > 3

  eth_clock_mode = clock_mode;

  tcpipInit();

  if (use_mac_from_efuse) {
    uint8_t p[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    esp_efuse_mac_get_custom(p);
    esp_base_mac_addr_set(p);
  }

  tcpip_adapter_set_default_eth_handlers();

  esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
  eth_netif = esp_netif_new(&cfg);

  esp_eth_mac_t* eth_mac = NULL;
#if CONFIG_ETH_SPI_ETHERNET_DM9051
  if (type == ETH_PHY_DM9051) {
    return false; // todo
  } else {
#endif
#if CONFIG_ETH_USE_ESP32_EMAC
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.clock_config.rmii.clock_mode = (eth_clock_mode) ? EMAC_CLK_OUT : EMAC_CLK_EXT_IN;
    mac_config.clock_config.rmii.clock_gpio = (1 == eth_clock_mode) ? EMAC_APPL_CLK_OUT_GPIO : (2 == eth_clock_mode) ? EMAC_CLK_OUT_GPIO
                                                                                             : (3 == eth_clock_mode) ? EMAC_CLK_OUT_180_GPIO
                                                                                                                     : EMAC_CLK_IN_GPIO;
    mac_config.smi_mdc_gpio_num = mdc;
    mac_config.smi_mdio_gpio_num = mdio;
    mac_config.sw_reset_timeout_ms = 1000;
    eth_mac = esp_eth_mac_new_esp32(&mac_config);
#endif
#if CONFIG_ETH_SPI_ETHERNET_DM9051
  }
#endif

  if (eth_mac == NULL) {
    log_e("esp_eth_mac_new_esp32 failed");
    return false;
  }

  eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
  phy_config.phy_addr = phy_addr;
  phy_config.reset_gpio_num = power;
  esp_eth_phy_t* eth_phy = NULL;
  switch (type) {
    case ETH_PHY_LAN8720:
      eth_phy = esp_eth_phy_new_lan8720(&phy_config);
      break;
    case ETH_PHY_TLK110:
      eth_phy = esp_eth_phy_new_ip101(&phy_config);
      break;
    case ETH_PHY_RTL8201:
      eth_phy = esp_eth_phy_new_rtl8201(&phy_config);
      break;
    case ETH_PHY_DP83848:
      eth_phy = esp_eth_phy_new_dp83848(&phy_config);
      break;
#if CONFIG_ETH_SPI_ETHERNET_DM9051
    case ETH_PHY_DM9051:
      eth_phy = esp_eth_phy_new_dm9051(&phy_config);
      break;
#endif
    case ETH_PHY_KSZ8041:
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
      eth_phy = esp_eth_phy_new_ksz8041(&phy_config);
#else
      log_e("unsupported ethernet type 'ETH_PHY_KSZ8041'");
#endif
      break;
    case ETH_PHY_KSZ8081:
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
      eth_phy = esp_eth_phy_new_ksz8081(&phy_config);
#else
      log_e("unsupported ethernet type 'ETH_PHY_KSZ8081'");
#endif
      break;
    default:
      break;
  }
  if (eth_phy == NULL) {
    log_e("esp_eth_phy_new failed");
    return false;
  }

  eth_handle = NULL;
  esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(eth_mac, eth_phy);
  // eth_config.on_lowlevel_init_done = on_lowlevel_init_done;
  // eth_config.on_lowlevel_deinit_done = on_lowlevel_deinit_done;
  if (esp_eth_driver_install(&eth_config, &eth_handle) != ESP_OK || eth_handle == NULL) {
    log_e("esp_eth_driver_install failed");
    return false;
  }

  /* attach Ethernet driver to TCP/IP stack */
  if (esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)) != ESP_OK) {
    log_e("esp_netif_attach failed");
    return false;
  }

  /* attach to WiFiGeneric to receive events */
  add_esp_interface_netif(ESP_IF_ETH, eth_netif);

  if (esp_eth_start(eth_handle) != ESP_OK) {
    log_e("esp_eth_start failed");
    return false;
  }
#else
  esp_err_t err;
  if (initialized) {
    err = esp_eth_enable();
    if (err) {
      log_e("esp_eth_enable error: %d", err);
      return false;
    }
    started = true;
    return true;
  }
  _eth_phy_mdc_pin = mdc;
  _eth_phy_mdio_pin = mdio;
  _eth_phy_power_pin = power;

  if (type == ETH_PHY_LAN8720) {
    eth_config_t config = phy_lan8720_default_ethernet_config;
    memcpy(&eth_config, &config, sizeof(eth_config_t));
  } else if (type == ETH_PHY_TLK110) {
    eth_config_t config = phy_tlk110_default_ethernet_config;
    memcpy(&eth_config, &config, sizeof(eth_config_t));
  } else if (type == ETH_PHY_IP101) {
    eth_config_t config = phy_ip101_default_ethernet_config;
    memcpy(&eth_config, &config, sizeof(eth_config_t));
  } else {
    log_e("Bad ETH_PHY type: %u", (uint8_t)type);
    return false;
  }

  eth_config.phy_addr = (eth_phy_base_t)phy_addr;
  eth_config.clock_mode = clock_mode;
  eth_config.gpio_config = _eth_phy_config_gpio;
  eth_config.tcpip_input = tcpip_adapter_eth_input;
  if (_eth_phy_power_pin >= 0) {
    _eth_phy_power_enable_orig = eth_config.phy_power_enable;
    eth_config.phy_power_enable = _eth_phy_power_enable;
  }

  tcpipInit();

  if (use_mac_from_efuse) {
    uint8_t p[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    esp_efuse_mac_get_custom(p);
    esp_base_mac_addr_set(p);
  }

  err = esp_eth_init(&eth_config);
  if (!err) {
    initialized = true;
    err = esp_eth_enable();
    if (err) {
      log_e("esp_eth_enable error: %d", err);
    } else {
      started = true;
      return true;
    }
  } else {
    log_e("esp_eth_init error: %d", err);
  }
#endif
  // holds a few milliseconds to let DHCP start and enter into a good state
  // FIX ME -- adresses issue https://github.com/espressif/arduino-esp32/issues/5733
  delay(50);

  return true;
}

bool ETHClass::config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1, IPAddress dns2) {

#if 0
    esp_err_t err = ESP_OK;
    tcpip_adapter_ip_info_t info;

    if (static_cast<uint32_t>(local_ip) != 0) {
        info.ip.addr = static_cast<uint32_t>(local_ip);
        info.gw.addr = static_cast<uint32_t>(gateway);
        info.netmask.addr = static_cast<uint32_t>(subnet);
    } else {
        info.ip.addr = 0;
        info.gw.addr = 0;
        info.netmask.addr = 0;
    }

    err = tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_ETH);
    if (err != ESP_OK && err != ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STOPPED) {
        log_e("DHCP could not be stopped! Error: %d", err);
        return false;
    }

    err = tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_ETH, &info);
    if (err != ERR_OK) {
        log_e("STA IP could not be configured! Error: %d", err);
        return false;
    }

    if (info.ip.addr) {
        staticIP = true;
    } else {
        err = tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_ETH);
        if (err != ESP_OK && err != ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STARTED) {
            log_w("DHCP could not be started! Error: %d", err);
            return false;
        }
        staticIP = false;
    }

    ip_addr_t d;
    d.type = IPADDR_TYPE_V4;

    if (static_cast<uint32_t>(dns1) != 0) {
        // Set DNS1-Server
        d.u_addr.ip4.addr = static_cast<uint32_t>(dns1);
        dns_setserver(0, &d);
    }

    if (static_cast<uint32_t>(dns2) != 0) {
        // Set DNS2-Server
        d.u_addr.ip4.addr = static_cast<uint32_t>(dns2);
        dns_setserver(1, &d);
    }
#else
  extern esp_err_t set_esp_interface_ip(esp_interface_t interface, IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dhcp_lease_start = INADDR_NONE);
  set_esp_interface_ip(ESP_IF_ETH, local_ip, gateway, subnet);
#endif
  return true;
}

IPAddress ETHClass::localIP() {
#if 0
    tcpip_adapter_ip_info_t ip;
    if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &ip)) {
        return IPAddress();
    }
    return IPAddress(ip.ip.addr);
#else
  esp_netif_ip_info_t ip;
  if (esp_netif_get_ip_info(eth_netif, &ip) == ESP_OK) {
    return IPAddress(ip.ip.addr);
  }
  return IPAddress();
#endif
}

IPAddress ETHClass::subnetMask() {
#if 0
    tcpip_adapter_ip_info_t ip;
    if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &ip)) {
        return IPAddress();
    }
    return IPAddress(ip.netmask.addr);
#else
  esp_netif_ip_info_t ip;
  if (esp_netif_get_ip_info(eth_netif, &ip) == ESP_OK) {
    return IPAddress(ip.netmask.addr);
  }
  return IPAddress();
#endif
}

IPAddress ETHClass::gatewayIP() {
#if 0
    tcpip_adapter_ip_info_t ip;
    if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &ip)) {
        return IPAddress();
    }
    return IPAddress(ip.gw.addr);
#else
  esp_netif_ip_info_t ip;
  if (esp_netif_get_ip_info(eth_netif, &ip) == ESP_OK) {
    return IPAddress(ip.gw.addr);
  }
  return IPAddress();
#endif
}

IPAddress ETHClass::dnsIP(uint8_t dns_no) {
  const ip_addr_t* dns_ip = dns_getserver(dns_no);
  return IPAddress(dns_ip->u_addr.ip4.addr);
}

IPAddress ETHClass::broadcastIP() {
  tcpip_adapter_ip_info_t ip;
  if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &ip)) {
    return IPAddress();
  }
  return WiFiGenericClass::calculateBroadcast(IPAddress(ip.gw.addr), IPAddress(ip.netmask.addr));
}

IPAddress ETHClass::networkID() {
  tcpip_adapter_ip_info_t ip;
  if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &ip)) {
    return IPAddress();
  }
  return WiFiGenericClass::calculateNetworkID(IPAddress(ip.gw.addr), IPAddress(ip.netmask.addr));
}

uint8_t ETHClass::subnetCIDR() {
  tcpip_adapter_ip_info_t ip;
  if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &ip)) {
    return (uint8_t)0;
  }
  return WiFiGenericClass::calculateSubnetCIDR(IPAddress(ip.netmask.addr));
}

const char* ETHClass::getHostname() {
  const char* hostname;
  if (tcpip_adapter_get_hostname(TCPIP_ADAPTER_IF_ETH, &hostname)) {
    return NULL;
  }
  return hostname;
}

bool ETHClass::setHostname(const char* hostname) {
  return tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_ETH, hostname) == 0;
}

bool ETHClass::fullDuplex() {
#if ESP_IDF_VERSION_MAJOR > 3
  eth_duplex_t link_duplex;
  esp_eth_ioctl(eth_handle, ETH_CMD_G_DUPLEX_MODE, &link_duplex);
  return (link_duplex == ETH_DUPLEX_FULL);
#else
  return eth_config.phy_get_duplex_mode();
#endif
}

bool ETHClass::linkUp() {
#if ESP_IDF_VERSION_MAJOR > 3
  return WiFiGenericClass::getStatusBits() & ETH_CONNECTED_BIT;
#else
  return eth_config.phy_check_link();
#endif
}

uint8_t ETHClass::linkSpeed() {
#if ESP_IDF_VERSION_MAJOR > 3
  eth_speed_t link_speed;
  esp_eth_ioctl(eth_handle, ETH_CMD_G_SPEED, &link_speed);
  return (link_speed == ETH_SPEED_10M) ? 10 : 100;
#else
  return eth_config.phy_get_speed_mode() ? 100 : 10;
#endif
}

bool ETHClass::enableIpV6() {
  return tcpip_adapter_create_ip6_linklocal(TCPIP_ADAPTER_IF_ETH) == 0;
}

IPv6Address ETHClass::localIPv6() {
  static ip6_addr_t addr;
  if (tcpip_adapter_get_ip6_linklocal(TCPIP_ADAPTER_IF_ETH, &addr)) {
    return IPv6Address();
  }
  return IPv6Address(addr.addr);
}

uint8_t* ETHClass::macAddress(uint8_t* mac) {
  if (!mac) {
    return NULL;
  }
#ifdef ESP_IDF_VERSION_MAJOR
  esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac);
#else
  esp_eth_get_mac(mac);
#endif
  return mac;
}

ESPCONNECT_STRING ETHClass::macAddress(void) {
  uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
  char macStr[18] = {0};
  macAddress(mac);
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return macStr;
}

ETHClass ETH;

#endif
