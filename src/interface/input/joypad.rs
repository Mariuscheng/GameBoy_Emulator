#[derive(Default, Clone, Copy)]
pub struct JoypadState {
    pub start: bool,
    pub select: bool,
    pub b: bool,
    pub a: bool,
    pub down: bool,
    pub up: bool,
    pub left: bool,
    pub right: bool,
}

pub trait Joypad {
    fn get_state(&self) -> JoypadState;
    // 進階功能：手把支援
    fn get_gamepad_state(&self) -> Option<JoypadState> {
        // TODO: 取得手把狀態（如 XInput/DirectInput）
        None
    }
    // 進階功能：自訂鍵位
    fn set_key_mapping(&mut self, mapping: JoypadMapping) {
        // TODO: 設定自訂鍵位
    }
    // 進階功能：多玩家支援
    fn get_multi_player_state(&self) -> Vec<JoypadState> {
        // TODO: 取得多玩家狀態
        vec![]
    }
    // 進階功能：即時顯示按鍵狀態
    fn show_key_status(&self) {
        // TODO: 圖形介面顯示
    }
    // 進階功能：錄影/回放
    fn record_input(&mut self) {
        // TODO: 記錄按鍵操作
    }
    fn playback_input(&mut self) {
        // TODO: 回放按鍵操作
    }
}

// 進階：自訂鍵位結構
#[derive(Default, Clone)]
pub struct JoypadMapping {
    pub key_a: u32,
    pub key_b: u32,
    pub key_start: u32,
    pub key_select: u32,
    pub key_up: u32,
    pub key_down: u32,
    pub key_left: u32,
    pub key_right: u32,
}
