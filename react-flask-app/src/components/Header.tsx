import React from 'react';
import { Col, Row } from 'react-bootstrap';
import styled from '@emotion/styled';
import { SiteHiveLogoIcon } from 'components/Icons';

// Define a styled Row with rounded corners
const StyledRow = styled(Row)`
  border-radius: 8px;
`;

function Header() {
  return (
    <StyledRow className="p-3 mb-2 bg-primary text-white">
      <Col>
        <SiteHiveLogoIcon height={25} />
      </Col>
      <Col>TV Slider</Col>
      <Col>[{window.location.hostname}]</Col>
    </StyledRow>
  );
}

export default Header;
