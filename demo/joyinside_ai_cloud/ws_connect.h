#ifndef JOYSINDIE_CLOUD_H

enum {
    JOYINSIDE_CLOUD_CLIENT_EVENT_INIT,
    JOYINSIDE_CLOUD_CLIENT_EVENT_INFO_CHECK,
    JOYINSIDE_CLOUD_CLIENT_EVENT_CONNETTINT,
    JOYINSIDE_CLOUD_CLIENT_EVENT_CONNECTED,
    JOYINSIDE_CLOUD_CLIENT_EVENT_DISCONNECTED,
    JOYINSIDE_CLOUD_CLIENT_EVENT_ERROR,

    JOYINSIDE_CLOUD_CHAT_INTERRUPT,
    JOYINSIDE_CLOUD_CHAT_EXIT,
    JOYINSIDE_CLOUD_TTS_COMPLETE,
    
    JOYINSIDE_CLOUD_CHAT_ASR,
    JOYINISDE_CLOUD_CHAT_ASR_FINAL,
};

typedef void (*joyinside_cloud_event_cb)(int event, void *usr_ptr);

char *get_botid();
char *get_dev_token();
char *get_uid();

int joyinside_cloud_init(joyinside_cloud_event_cb event_cb, void *user_ptr);
void joyinside_cloud_deinit();
int joyinside_cloud_is_conneced();
int joyinside_cloud_connect();
int joyinside_cloud_disconnect();

void joyinside_cloud_chat_interrupt();

int joyinside_cloud_chat_update();

int joyinside_cloud_chat_reset();

int joyinside_cloud_text_to_speech(char *text);

void joyinside_cloud_audio_send_stop();
void joyinside_cloud_audio_send_start();

#endif