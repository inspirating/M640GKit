#include "freertos/semphr.h" // 必须包含这个头文件

namespace M640GKit {

class M640GPumpSimulator {
    public:
    M640GPumpSimulator() {
        // 在构造函数中初始化锁
        xSemaphore = xSemaphoreCreateMutex();
    }

    private:
    // 记录当前是否正在输注，防止重复启动线程
    volatile bool isDeliveryTaskRunning = false; 
    SemaphoreHandle_t xSemaphore; // 定义一个锁
};

// 这是跑在独立线程里的同步输注任务
static void M640GPumpSimulator::gpioDeliveryTask(void *parameter) {
    // 解析传入的 this 指针和步数参数
    M640GPumpSimulator* simulator = static_cast<M640GPumpSimulator*>(parameter);
    int steps = simulator->gpioRemainingSteps; 

    Logger::info("[Task] 独立输注线程启动，目标步数: " + String(steps));

    // ==========================================
    // 下面就是你测试成功的极其直观的同步阻塞逻辑！
    // ==========================================

    // 1. 唤醒屏幕
    digitalWrite(STEP_PIN, LOW);
    vTaskDelay(pdMS_TO_TICKS(200));   // FreeRTOS 标准的 delay 写法，不会阻塞主线程
    digitalWrite(STEP_PIN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(1000)); 

    // 2. 触发进入模式
    digitalWrite(STEP_PIN, LOW);
    vTaskDelay(pdMS_TO_TICKS(2000)); 
    digitalWrite(STEP_PIN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(1500)); 

    // 3. 循环输入步数
    for (int i = 1; i <= steps; i++) {
        digitalWrite(STEP_PIN, LOW);
        vTaskDelay(pdMS_TO_TICKS(400));
        digitalWrite(STEP_PIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(800)); // 这里的时序你自己测出完美的数值填进来
    }
    vTaskDelay(pdMS_TO_TICKS(1500)); 

    // 4. 第一次长按确认
    digitalWrite(STEP_PIN, LOW);
    vTaskDelay(pdMS_TO_TICKS(2000));
    digitalWrite(STEP_PIN, HIGH);
    // 根据步数动态等待泵响完（每步大概1秒）
    vTaskDelay(pdMS_TO_TICKS(2000 + steps * 1000)); 

    // 5. 第二次长按执行
    digitalWrite(STEP_PIN, LOW);
    vTaskDelay(pdMS_TO_TICKS(2000));
    digitalWrite(STEP_PIN, HIGH);

    Logger::info("[Task] 大剂量物理输入完毕！");

    // 标记任务结束，方便队列进行下一次调度
    simulator->isDeliveryTaskRunning = false;
    
    // 线程执行完毕，必须自行销毁！
    vTaskDelete(NULL); 
}

}