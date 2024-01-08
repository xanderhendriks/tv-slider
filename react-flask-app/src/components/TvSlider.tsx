import React from 'react';
import { Button } from 'react-bootstrap';


interface Props {
  position: number;
  handleMoveInClick: () => void;
  handleMoveOutClick: () => void;
  handleMoveStopClick: () => void;
}


function TvSlider({ position, handleMoveInClick, handleMoveOutClick, handleMoveStopClick }: Props) {
  const renderPosition = () => {
    let display;
    let containerStyle = {};
    let containerHiddenStyle = {};

    switch(position) {
      case 0:
        display = <div>Position Unknown</div>;
        break;
      case 1:
        display = <div>TV Invisible</div>;
        containerStyle = { width: '0px' };
        containerHiddenStyle = { width: '220px' };
        break;
      case 2:
        display = <div>TV 25% Visible</div>;
        containerStyle = { width: '55px' };
        containerHiddenStyle = { width: '165px' };
        break;
      case 3:
        display = <div>TV 50% Visible</div>;
        containerStyle = { width: '110px' };
        containerHiddenStyle = { width: '110px' };
        break;
      case 4:
        display = <div>TV 75% Visible</div>;
        containerStyle = { width: '165px' };
        containerHiddenStyle = { width: '55px' };
        break;
      case 5:
        display = <div>TV 100% Visible</div>;
        containerStyle = { width: '220px' };
        containerHiddenStyle = { width: '0px' };
        break;
      default:
        display = <div>Invalid Position</div>;
    }

    return (
      <div style={{ border: '1px solid #ccc', borderRadius: '10px', padding: '10px', marginTop: '10px', textAlign: 'center' }}>
        <div >
          <img src="/sliding_tv.png" alt="TV" width="220" height="150" style={{ objectFit: 'cover', objectPosition: '0 100%', ...containerStyle }} />
          <img src="/hidden_tv.png" alt="Hidden TV" width="220" height="150" style={{ objectFit: 'cover', objectPosition: '100% 0', ...containerHiddenStyle }} />
        </div>
        {display}
      </div>
    );
  };

  return (
    <div>
      {renderPosition()}
      <div style={{ display: 'flex', justifyContent: 'space-between', border: '1px solid #ccc', borderRadius: '10px', padding: '10px', marginTop: '10px', textAlign: 'center' }}>
        <Button onClick={handleMoveOutClick} style={{width: 100}}>&lt;&lt;</Button>
        <Button onClick={handleMoveStopClick} style={{width: 100}}>Stop</Button>
        <Button onClick={handleMoveInClick} style={{width: 100}}>&gt;&gt;</Button>
      </div>
    </div>
  );
}

export default TvSlider;
