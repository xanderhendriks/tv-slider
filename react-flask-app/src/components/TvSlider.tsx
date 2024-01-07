import React from 'react';
import { Button } from 'react-bootstrap';

interface Props {
  handleMoveInClick: () => void;
  handleMoveOutClick: () => void;
  handleMoveStopClick: () => void;
}

function TvSlider({ handleMoveInClick, handleMoveOutClick, handleMoveStopClick }: Props) {
  return (
    <div>
      <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '10px' }}>
        <Button onClick={handleMoveOutClick}>Move Out</Button>
        <Button onClick={handleMoveStopClick}>Stop</Button>
        <Button onClick={handleMoveInClick}>Move In</Button>
      </div>
    </div>
  );
}

export default TvSlider;
