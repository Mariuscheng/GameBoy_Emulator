use std::fmt;

/// 音頻介面特徵
pub trait AudioInterface: fmt::Debug {
    /// 將音頻樣本添加到播放佇列
    fn queue_samples(&mut self, samples: &[f32]);

    /// 恢復音頻播放
    fn resume(&mut self);

    /// 暫停音頻播放
    fn pause(&mut self);
}

// 測試用的空音頻輸出
#[derive(Debug, Default)]
pub struct NullAudioOutput;

impl AudioInterface for NullAudioOutput {
    fn queue_samples(&mut self, _samples: &[f32]) {}
    fn resume(&mut self) {}
    fn pause(&mut self) {}
}
