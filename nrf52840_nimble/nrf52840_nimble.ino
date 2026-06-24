// 极简裸机 LED 闪灯测试 - 不依赖任何库
// 排除 NimBLE / pump_simulator / USB CDC 等干扰

void setup() {
    // 直接操作 nRF52 GPIO 寄存器, 不依赖 Arduino API
    // P0.15 = Nice!Nano 板载 LED (active-low)
    NRF_P0->PIN_CNF[15] = 1;  // 输出模式 (DIR=1)
    NRF_P0->OUTSET = (1UL << 15);  // 灭 (active-low, HIGH=灭)
}

void loop() {
    NRF_P0->OUTCLR = (1UL << 15);  // 亮
    for (volatile int i = 0; i < 500000; i++);  // 粗延时
    NRF_P0->OUTSET = (1UL << 15);  // 灭
    for (volatile int i = 0; i < 500000; i++);  // 粗延时
}
