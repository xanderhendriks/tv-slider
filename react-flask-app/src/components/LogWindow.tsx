import React, { useEffect, useRef } from 'react';
import styled from '@emotion/styled';

const Container = styled.div`
  background: blanchedalmond;
  padding: 4px;
  display: flex;
  flex-direction: column;
  flex: 1;
  height: 150px;
  overflow-y: scroll;
  border: 1px solid black;
  margin-top: 10px;
  border-radius: 8px;
`;

const Line = styled.span`
  display: block;
`;

interface Props {
  log?: string[];
}

function LogWindow({ log = ['No logs...'] }: Props) {
  const windowRef = useRef<any>(null);

  useEffect(() => {
    if (windowRef.current) {
      windowRef.current.scrollTop = windowRef.current.scrollHeight;
    }
  }, [log]);

  return (
    <Container ref={windowRef}>
      {log.map((line, i) => (
        <Line key={i}>{line}</Line>
      ))}
    </Container>
  );
}

export default LogWindow;
