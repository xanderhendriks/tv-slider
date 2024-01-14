import React, { useState, useEffect } from 'react';
import { Row, Container } from 'react-bootstrap';
import { LogWindow, Header, TvSlider } from 'components';
import axios from 'axios';
import styled from '@emotion/styled';

import 'bootstrap/dist/css/bootstrap.css';
import 'bootstrap/dist/css/bootstrap.min.css';

const Wrapper = styled.div`
  height: 100vh;
  display: flex;
  flex-direction: column;
`;

const FillRow = styled(Row)`
  flex: 1;
`;

function App() {
  const [logs, setLogs] = useState<string[]>([]);
  const [position, setPosition] = useState(0);

  const handleMoveInClick = async () => {
    await axios.get('/api/move/in');
  };

  const handleMoveOutClick = async () => {
    await axios.get('/api/move/out');
  };

  const handleMoveStopClick = async () => {
    await axios.get('/api/move/stop');
  };

  useEffect(() => {
    const source = new EventSource("http://" + window.location.hostname + ":5000/stream");
    source.addEventListener('position', handlePositionMessage, false);
    source.addEventListener('message', handleLogMessage, false);
    source.addEventListener('error', handleRedisError, false);
  
    return () => {
      source.removeEventListener('position', handlePositionMessage, false);
      source.removeEventListener('message', handleLogMessage, false);
      source.removeEventListener('error', handleRedisError, false);
    };
  }, []);

  useEffect(() => {
    axios.get('/api/position/get')
      .then(response => {
        setPosition(response.data);
      })
      .catch(error => console.error('Error fetching battery config:', error));
  }, []);  


  return (
    <Wrapper>
      <Container
        style={{ display: 'flex', flexDirection: 'column', height: '100%'}}
      >
        <Header />
        <FillRow>
          <TvSlider
            position={position}
            handleMoveInClick={handleMoveInClick}
            handleMoveOutClick={handleMoveOutClick}
            handleMoveStopClick={handleMoveStopClick}
          />
          <LogWindow log={logs} />
        </FillRow>
      </Container>
    </Wrapper>
  );

  function handlePositionMessage(event: any) {
    setPosition(event.data);
  }

  function handleLogMessage(event: any) {
    const data = JSON.parse(event.data);
    console.log(data.message);
    setLogs(logs => [
      ...logs,
      data.message
    ]);
  }

  function handleRedisError(event: any) {
    console.log("Error", event)
    setLogs(logs => [
      ...logs,
      "Failed to connect to event stream. Is Redis running?"
    ]);
  }
}

export default App;
