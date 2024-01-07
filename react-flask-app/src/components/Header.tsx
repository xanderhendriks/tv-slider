import React from 'react';
import { Col, Row } from 'react-bootstrap';
import { SiteHiveLogoIcon } from 'components/Icons';

function Header() {
  return (
    <Row className="p-3 mb-2 bg-primary text-white">
      <Col>
        <SiteHiveLogoIcon height={25} />
      </Col>
      <Col>Camera Focus Jig</Col>
      <Col>[foc-jig-1]</Col>
    </Row>
  );
}

export default Header;
