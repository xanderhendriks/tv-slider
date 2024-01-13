import React from 'react';
import { Button } from 'react-bootstrap';

interface Props {
  position: number; // -1 for unknown, 0 to 100 for percentage visibility
  handleMoveInClick: () => void;
  handleMoveOutClick: () => void;
  handleMoveStopClick: () => void;
}

function TvSlider({ position, handleMoveInClick, handleMoveOutClick, handleMoveStopClick }: Props) {
  const renderPosition = () => {
    let display;
    let containerStyle = {};
    let containerHiddenStyle = {};

    if (position >= 0 && position <= 100) {
      const visibleWidth = (220 * position) / 100;
      const hiddenWidth = 220 - visibleWidth;
      containerStyle = { width: `${visibleWidth}px` };
      containerHiddenStyle = { width: `${hiddenWidth}px` };
      display = <div>TV {position}% Visible</div>;
      return (
        <div style={{ border: '1px solid #ccc', borderRadius: '10px', padding: '10px', marginTop: '10px', textAlign: 'center' }}>
          <div>
            <img src="/sliding_tv.png" alt="TV" width="220" height="150" style={{ objectFit: 'cover', objectPosition: '0 100%', ...containerStyle }} />
            <img src="/hidden_tv.png" alt="Hidden TV" width="220" height="150" style={{ objectFit: 'cover', objectPosition: '100% 0', ...containerHiddenStyle }} />
          </div>
          {display}
        </div>
      );
    } else {
      display = <div>Unknown position</div>;
      return (
        <div style={{ border: '1px solid #ccc', borderRadius: '10px', padding: '10px', marginTop: '10px', textAlign: 'center' }}>
          <div>
            <img src="/unknown_tv.png" alt="Unknown TV" width="220" height="150" />
          </div>
          {display}
        </div>
      );
    }
  };

  return (
    <div>
      {renderPosition()}
      <div style={{ display: 'flex', justifyContent: 'space-between', border: '1px solid #ccc', borderRadius: '10px', padding: '10px', marginTop: '10px', textAlign: 'center' }}>
        <Button onClick={handleMoveOutClick} style={{ width: 100 }}>&lt;&lt;</Button>
        <Button onClick={handleMoveStopClick} style={{ width: 100 }}>Stop</Button>
        <Button onClick={handleMoveInClick} style={{ width: 100 }}>&gt;&gt;</Button>
      </div>
    </div>
  );
}

export default TvSlider;
