#include <stdint.h>
#include "esp_check.h"
#include "esp_log.h"
#include "bsp_board.h"
#include "bsp_gpio.h"
#include "driver/gpio.h"

struct _bsp_gpio_input
{
    uint8_t id;
    void (*cb)(uint8_t id);
    struct _bsp_gpio_input *next;
};

static struct _bsp_gpio_input *g_bsp_gpio_input_tb = NULL;

void bsp_gpio_input_register_cb(uint8_t id,void (*cb)(uint8_t id))
{
    struct _bsp_gpio_input *head = g_bsp_gpio_input_tb;
    struct _bsp_gpio_input *head_next = g_bsp_gpio_input_tb;
    while(head_next != NULL)
    {
        if(head->id == id)
        {
            return;
        }
        head = head_next;
        head_next = head->next;
    }

    if(head == g_bsp_gpio_input_tb)
    {
        g_bsp_gpio_input_tb = malloc(sizeof(struct _bsp_gpio_input));
        g_bsp_gpio_input_tb->id = id;
        g_bsp_gpio_input_tb->cb = cb;
        g_bsp_gpio_input_tb->next = NULL;
    }
    else
    {
        head->next = malloc(sizeof(struct _bsp_gpio_input));
        head->next->id = id;
        head->next->cb = cb;
        head->next->next = NULL;
    }
}

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    struct _bsp_gpio_input *head = g_bsp_gpio_input_tb;
    const board_res_desc_t *brd = bsp_board_get_description();
    uint32_t gpio_num = (uint32_t) arg;
    uint8_t id = 0;

    for(uint8_t i = 0;i<brd->gpio_input_tb_len;i++)
    {
        if(brd->gpio_input_tb[i].gpio_num == gpio_num)
        {
            id = i;
        }
    }
    while(head != NULL)
    {
        if(head->id == id && head->cb != NULL)
        {
            head->cb(id);
        }
        head = head->next;
    }
}

void bsp_gpio_isr_add(uint8_t id)
{
    const board_res_desc_t *brd = bsp_board_get_description();
    
    if(brd->gpio_input_tb_len > id){
        gpio_set_intr_type(brd->gpio_input_tb[id].gpio_num, brd->gpio_input_tb[id].type);
        gpio_isr_handler_add(brd->gpio_input_tb[id].gpio_num, gpio_isr_handler, (void*) (brd->gpio_input_tb[id].gpio_num));
    }
}

void bsp_gpio_isr_remove(uint8_t id)
{
    const board_res_desc_t *brd = bsp_board_get_description();
    if(brd->gpio_input_tb_len > id)
        gpio_isr_handler_remove(brd->gpio_input_tb[id].gpio_num);
}

esp_err_t bsp_gpio_init(int32_t gpio_num,gpio_mode_t mode,bool active_level,gpio_int_type_t intr_type)
{
    gpio_config_t gpio_conf;
    gpio_conf.intr_type = intr_type;
    gpio_conf.mode = mode;
    gpio_conf.pin_bit_mask = (1ULL << gpio_num);
    gpio_conf.pull_down_en = GPIO_PULLUP_DISABLE;
    gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&gpio_conf);
    if(intr_type != GPIO_INTR_DISABLE)
    {
        gpio_install_isr_service(0);
        //hook isr handler for specific gpio pin 
    }

    return ESP_OK;
}

void bsp_gpio_base_set(int32_t gpio_num,bool value)
{
    gpio_set_level(gpio_num,value);
}

void bsp_gpio_set(uint8_t id,bool value)
{
    const board_res_desc_t *brd = bsp_board_get_description();
    if(brd->gpio_tb_len > id)
    {
        gpio_set_level(brd->gpio_tb[id],value);
    }
}

int bsp_gpio_get(uint8_t id)
{
    const board_res_desc_t *brd = bsp_board_get_description();
    if(brd->gpio_tb_len > id)
    {
        return gpio_get_level(brd->gpio_tb[id]);
    }
    return -1;
}

void bsp_gpio_input_init(void)
{
    const board_res_desc_t *brd = bsp_board_get_description();
    for(uint8_t i = 0;i<brd->gpio_input_tb_len;i++)
    {
        printf("%s gpio_num:%d type:%d\r\n",__FUNCTION__,brd->gpio_input_tb[i].gpio_num,brd->gpio_input_tb[i].type);
        bsp_gpio_init(brd->gpio_input_tb[i].gpio_num,GPIO_MODE_INPUT,1,brd->gpio_input_tb[i].type);
    }

}
