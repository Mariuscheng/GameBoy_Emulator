pub trait Joypad {
    // TODO: 根據需求補充 Joypad trait 方法
    fn is_start_pressed(&self) -> bool { false }
    fn is_select_pressed(&self) -> bool { false }
    fn is_b_pressed(&self) -> bool { false }
    fn is_a_pressed(&self) -> bool { false }
    fn is_down_pressed(&self) -> bool { false }
    fn is_up_pressed(&self) -> bool { false }
    fn is_left_pressed(&self) -> bool { false }
    fn is_right_pressed(&self) -> bool { false }
}
