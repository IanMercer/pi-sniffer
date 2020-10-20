import React, { Component } from 'react';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome'
import { faBolt, faHeartbeat, faLaptop, faMobileAlt } from '@fortawesome/free-solid-svg-icons'
import { faClock } from '@fortawesome/free-solid-svg-icons'

const cellphone = <span style={{padding:'0px 3px 0px 0px'}}><FontAwesomeIcon icon={faMobileAlt}/></span>
const clock = <span style={{padding:'0px 3px 0px 0px'}}><FontAwesomeIcon icon={faClock}/></span>
const computer = <span style={{padding:'0px 3px 0px 0px'}}><FontAwesomeIcon icon={faLaptop}/></span>
const wearable = <span style={{padding:'0px 3px 0px 0px'}}><FontAwesomeIcon icon={faHeartbeat}/></span>
const beacon = <span style={{padding:'0px 3px 0px 0px'}}><FontAwesomeIcon icon={faBolt}/></span>

class App extends Component {

  state = {
    assets : [],
    rooms: [],
    groups: [],
    fill_a: '#ff0000',
    fill_b: '#f8fff8',
    figures: ['#ff0000','#ff0000','#00ff00','#ff00ff','#ffee00','#ff1100'],
    scale_factor: 0.5
  }

  constructor(props)
  {
    super(props);
    this.timer = this.timer.bind(this);
  }

  componentDidMount() {
    var interval = setInterval(this.timer, 1000);
    this.setState({interval: interval});
  }

 componentWillUnmount () {
    clearInterval(this.state.interval);
 }
 
 timer()
 {
    var self = this;
    // setState method is used to update the state
    fetch('/cgi-bin/cgijson.cgi')
    .then(res => res.json())
    .then((data) => {
      self.setState({ rooms: data.rooms.sort((a, b) => (''+a.group+'_'+a.name).localeCompare(b.group+'_'+b.name)) })
      self.setState({ assets: data.assets })
      self.setState({ groups: data.groups })

      if (data.signage.scale_factor) self.setState({scale_factor: data.signage.scale_factor});

      let max = 0;
      self.state.groups.map(g => { if (g.phones > max) max = g.phones;});

      max = max * self.state.scale_factor;

      let colors = [];
      let i = 0;
      for (i = 0; i < 6; i++)
      {
        if (i < max) 
        {
          if (max < 3)
             colors.push("#00ff00");
          else if (max < 5)
             colors.push("#eeee00");
          else 
             colors[i] = "#ff0000";
        }
        else colors[i] = "#e0e0e0";
      }
      self.setState({ figures: colors });

      self.state.rooms.map((x) => {


        let color = "#ff0000";
        if (x.phones <1) color = '#80ff80';
        else if (x.phones <2) color = '#3f3f80';
        else if (x.phones <3) color = '#ffff80';

        if (x.name === "Kitchen"){
          this.setState({fill_b : color});
        }
        if (x.name === "LivingRoom"){
          this.setState({fill_a : color});
        }
        //console.log(x);
        return x;
      });

      //console.log(this.state)
    })
    .catch(console.log)
 }

  repeat(e, count)
  {
    var result = [];
    for(var i = 0; i < count; i++) {
        result.push(<span key={i} style={{padding:'0px 3px 0px 0px'}}>{e}</span>);
    }
    return result;
  }

  render() {
    return (
      <div className="container-fluid pt-3">

      <h2>Groups</h2>
      <div className="d-flex flex-wrap">
        {this.state.groups.map((x) => (
          <div className="card" style={{width: '18rem'}} key={x.name}>
            <div className={"card-header "+ ((x.phones ==null || x.phones < 2.5) ? "bg-success":"bg-danger")}>{x.name}</div>
            <div className="card-body">
            <h6 className="card-subtitle mb-2 text-muted">
                {this.repeat(cellphone, x.phones)}
                {this.repeat(clock, x.watches)}
                {this.repeat(wearable, x.wearables)}
                {this.repeat(computer, x.computers)}
              </h6>
              <h6 className="card-subtitle mb-2 text-muted">
                {this.repeat(beacon, x.beacons)}
              </h6>
            </div>
          </div>
        ))}
      </div>

      <h2>Rooms</h2>
      <div className="d-flex flex-wrap">
        {this.state.rooms.map((x) => (
          <div className="card" style={{width: '18rem'}} key={x.name}>
            <div className={"card-header "+ ((x.phones ==null || x.phones < 2.5) ? "bg-success":"bg-danger")}>{x.name}</div>
            <div className="card-body">
              <h6 className="card-subtitle mb-2 text-muted">
              {x.phones > 0 ? <span>{cellphone}{Math.round(x.phones)}&nbsp;</span> : ''} 
              {x.watches > 0 ? <span>{clock}{Math.round(x.watches)}&nbsp;</span> : ''} 
              {x.wearables > 0 ? <span>{wearable}{Math.round(x.wearables)}&nbsp;</span> : ''} 
              {x.computers > 0 ? <span>{computer}{Math.round(x.computers)}&nbsp;</span> : ''} 
              {x.beacons > 0 ? <span>{beacon}{Math.round(x.beacons)}&nbsp;</span> : ''} 
              </h6>
            </div>
          </div>
        ))}
      </div>

      <h2>Assets</h2>
      <div className="d-flex flex-wrap">
        {this.state.assets.map((x) => (
          <div className="card" style={{width: '18rem'}} key={x.name}>
            <div className={"card-header "+ ((x.ago === 'now') ? "bg-success":"bg-danger")}>{x.name}</div>
            <div className="card-body">
              <h6 className="card-subtitle mb-2 text-muted">
              <span> {x.room}</span>
              <span style={{float:'right'}}> {x.ago}</span>
              </h6>
            </div>
          </div>
        ))}
      </div>

<h2>Floor plan</h2>
<div>

<p> You can create SVG using draw.io and then animate it using ReactJS </p>

<svg xmlns="http://www.w3.org/2000/svg"
    xmlnsXlink="http://www.w3.org/1999/xlink" version="1.1" width="400" height="300" viewBox="-0.5 -0.5 1581 1211">
    <defs/>
    <g>
        <rect x="1028" y="366" width="369" height="461" fill="#d5e8d4" stroke="#82b366"/>
        <rect x="10" y="1149" width="300" height="50" fill="#ffffff" stroke="#000000"/>
        <path d="M 1285.44 1048.28 L 1288.43 1072.36 C 1289 1075.89 1287.44 1079.44 1284.45 1081.38 C 1274.21 1083 1263.79 1083 1253.55 1081.38 C 1250.56 1079.44 1249 1075.89 1249.57 1072.36 L 1252.56 1048.28 C 1253.13 1045.61 1255.98 1043.56 1259.53 1043.26 L 1278.47 1043.26 C 1282.02 1043.56 1284.87 1045.61 1285.44 1048.28 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10"/>
        <path d="M 1284.67 1042.77 C 1285.49 1043.08 1285.94 1044.08 1285.67 1045.01 C 1285.39 1045.94 1284.5 1046.44 1283.67 1046.13 C 1273.82 1043.7 1263.62 1043.7 1253.77 1046.13 C 1253.15 1045.95 1252.66 1045.43 1252.46 1044.75 C 1252.25 1044.07 1252.38 1043.33 1252.78 1042.77 C 1263.26 1040 1274.18 1040 1284.67 1042.77" fill="#ffffff" stroke="#000000" strokeMiterlimit="10"/>
        <path d="M 1285.44 1145.28 L 1288.43 1169.36 C 1289 1172.89 1287.44 1176.44 1284.45 1178.38 C 1274.21 1180 1263.79 1180 1253.55 1178.38 C 1250.56 1176.44 1249 1172.89 1249.57 1169.36 L 1252.56 1145.28 C 1253.13 1142.61 1255.98 1140.56 1259.53 1140.26 L 1278.47 1140.26 C 1282.02 1140.56 1284.87 1142.61 1285.44 1145.28 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(180,1269,1158.5)"/>
        <path d="M 1284.67 1139.77 C 1285.49 1140.08 1285.94 1141.08 1285.67 1142.01 C 1285.39 1142.94 1284.5 1143.44 1283.67 1143.13 C 1273.82 1140.7 1263.62 1140.7 1253.77 1143.13 C 1253.15 1142.95 1252.66 1142.43 1252.46 1141.75 C 1252.25 1141.07 1252.38 1140.33 1252.78 1139.77 C 1263.26 1137 1274.18 1137 1284.67 1139.77" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(180,1269,1158.5)"/>
        <path d="M 1236.94 1096.78 L 1239.93 1120.86 C 1240.5 1124.39 1238.94 1127.94 1235.95 1129.88 C 1225.71 1131.5 1215.29 1131.5 1205.05 1129.88 C 1202.06 1127.94 1200.5 1124.39 1201.07 1120.86 L 1204.06 1096.78 C 1204.63 1094.11 1207.48 1092.06 1211.03 1091.76 L 1229.97 1091.76 C 1233.52 1092.06 1236.37 1094.11 1236.94 1096.78 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(270,1220.5,1110)"/>
        <path d="M 1236.17 1091.27 C 1236.99 1091.58 1237.44 1092.58 1237.17 1093.51 C 1236.89 1094.44 1236 1094.94 1235.17 1094.63 C 1225.32 1092.2 1215.12 1092.2 1205.27 1094.63 C 1204.65 1094.45 1204.16 1093.93 1203.96 1093.25 C 1203.75 1092.57 1203.88 1091.83 1204.28 1091.27 C 1214.76 1088.5 1225.68 1088.5 1236.17 1091.27" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(270,1220.5,1110)"/>
        <path d="M 1513.94 1096.78 L 1516.93 1120.86 C 1517.5 1124.39 1515.94 1127.94 1512.95 1129.88 C 1502.71 1131.5 1492.29 1131.5 1482.05 1129.88 C 1479.06 1127.94 1477.5 1124.39 1478.07 1120.86 L 1481.06 1096.78 C 1481.63 1094.11 1484.48 1092.06 1488.03 1091.76 L 1506.97 1091.76 C 1510.52 1092.06 1513.37 1094.11 1513.94 1096.78 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,1497.5,1110)"/>
        <path d="M 1513.17 1091.27 C 1513.99 1091.58 1514.44 1092.58 1514.17 1093.51 C 1513.89 1094.44 1513 1094.94 1512.17 1094.63 C 1502.32 1092.2 1492.12 1092.2 1482.27 1094.63 C 1481.65 1094.45 1481.16 1093.93 1480.96 1093.25 C 1480.75 1092.57 1480.88 1091.83 1481.28 1091.27 C 1491.76 1088.5 1502.68 1088.5 1513.17 1091.27" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,1497.5,1110)"/>
        <path d="M 1345.44 1048.28 L 1348.43 1072.36 C 1349 1075.89 1347.44 1079.44 1344.45 1081.38 C 1334.21 1083 1323.79 1083 1313.55 1081.38 C 1310.56 1079.44 1309 1075.89 1309.57 1072.36 L 1312.56 1048.28 C 1313.13 1045.61 1315.98 1043.56 1319.53 1043.26 L 1338.47 1043.26 C 1342.02 1043.56 1344.87 1045.61 1345.44 1048.28 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10"/>
        <path d="M 1344.67 1042.77 C 1345.49 1043.08 1345.94 1044.08 1345.67 1045.01 C 1345.39 1045.94 1344.5 1046.44 1343.67 1046.13 C 1333.82 1043.7 1323.62 1043.7 1313.77 1046.13 C 1313.15 1045.95 1312.66 1045.43 1312.46 1044.75 C 1312.25 1044.07 1312.38 1043.33 1312.78 1042.77 C 1323.26 1040 1334.18 1040 1344.67 1042.77" fill="#ffffff" stroke="#000000" strokeMiterlimit="10"/>
        <path d="M 1345.44 1145.28 L 1348.43 1169.36 C 1349 1172.89 1347.44 1176.44 1344.45 1178.38 C 1334.21 1180 1323.79 1180 1313.55 1178.38 C 1310.56 1176.44 1309 1172.89 1309.57 1169.36 L 1312.56 1145.28 C 1313.13 1142.61 1315.98 1140.56 1319.53 1140.26 L 1338.47 1140.26 C 1342.02 1140.56 1344.87 1142.61 1345.44 1145.28 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(180,1329,1158.5)"/>
        <path d="M 1344.67 1139.77 C 1345.49 1140.08 1345.94 1141.08 1345.67 1142.01 C 1345.39 1142.94 1344.5 1143.44 1343.67 1143.13 C 1333.82 1140.7 1323.62 1140.7 1313.77 1143.13 C 1313.15 1142.95 1312.66 1142.43 1312.46 1141.75 C 1312.25 1141.07 1312.38 1140.33 1312.78 1139.77 C 1323.26 1137 1334.18 1137 1344.67 1139.77" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(180,1329,1158.5)"/>
        <path d="M 1405.44 1048.28 L 1408.43 1072.36 C 1409 1075.89 1407.44 1079.44 1404.45 1081.38 C 1394.21 1083 1383.79 1083 1373.55 1081.38 C 1370.56 1079.44 1369 1075.89 1369.57 1072.36 L 1372.56 1048.28 C 1373.13 1045.61 1375.98 1043.56 1379.53 1043.26 L 1398.47 1043.26 C 1402.02 1043.56 1404.87 1045.61 1405.44 1048.28 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10"/>
        <path d="M 1404.67 1042.77 C 1405.49 1043.08 1405.94 1044.08 1405.67 1045.01 C 1405.39 1045.94 1404.5 1046.44 1403.67 1046.13 C 1393.82 1043.7 1383.62 1043.7 1373.77 1046.13 C 1373.15 1045.95 1372.66 1045.43 1372.46 1044.75 C 1372.25 1044.07 1372.38 1043.33 1372.78 1042.77 C 1383.26 1040 1394.18 1040 1404.67 1042.77" fill="#ffffff" stroke="#000000" strokeMiterlimit="10"/>
        <path d="M 1405.44 1145.28 L 1408.43 1169.36 C 1409 1172.89 1407.44 1176.44 1404.45 1178.38 C 1394.21 1180 1383.79 1180 1373.55 1178.38 C 1370.56 1176.44 1369 1172.89 1369.57 1169.36 L 1372.56 1145.28 C 1373.13 1142.61 1375.98 1140.56 1379.53 1140.26 L 1398.47 1140.26 C 1402.02 1140.56 1404.87 1142.61 1405.44 1145.28 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(180,1389,1158.5)"/>
        <path d="M 1404.67 1139.77 C 1405.49 1140.08 1405.94 1141.08 1405.67 1142.01 C 1405.39 1142.94 1404.5 1143.44 1403.67 1143.13 C 1393.82 1140.7 1383.62 1140.7 1373.77 1143.13 C 1373.15 1142.95 1372.66 1142.43 1372.46 1141.75 C 1372.25 1141.07 1372.38 1140.33 1372.78 1139.77 C 1383.26 1137 1394.18 1137 1404.67 1139.77" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(180,1389,1158.5)"/>
        <path d="M 1465.44 1048.28 L 1468.43 1072.36 C 1469 1075.89 1467.44 1079.44 1464.45 1081.38 C 1454.21 1083 1443.79 1083 1433.55 1081.38 C 1430.56 1079.44 1429 1075.89 1429.57 1072.36 L 1432.56 1048.28 C 1433.13 1045.61 1435.98 1043.56 1439.53 1043.26 L 1458.47 1043.26 C 1462.02 1043.56 1464.87 1045.61 1465.44 1048.28 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10"/>
        <path d="M 1464.67 1042.77 C 1465.49 1043.08 1465.94 1044.08 1465.67 1045.01 C 1465.39 1045.94 1464.5 1046.44 1463.67 1046.13 C 1453.82 1043.7 1443.62 1043.7 1433.77 1046.13 C 1433.15 1045.95 1432.66 1045.43 1432.46 1044.75 C 1432.25 1044.07 1432.38 1043.33 1432.78 1042.77 C 1443.26 1040 1454.18 1040 1464.67 1042.77" fill="#ffffff" stroke="#000000" strokeMiterlimit="10"/>
        <path d="M 1465.44 1145.28 L 1468.43 1169.36 C 1469 1172.89 1467.44 1176.44 1464.45 1178.38 C 1454.21 1180 1443.79 1180 1433.55 1178.38 C 1430.56 1176.44 1429 1172.89 1429.57 1169.36 L 1432.56 1145.28 C 1433.13 1142.61 1435.98 1140.56 1439.53 1140.26 L 1458.47 1140.26 C 1462.02 1140.56 1464.87 1142.61 1465.44 1145.28 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(180,1449,1158.5)"/>
        <path d="M 1464.67 1139.77 C 1465.49 1140.08 1465.94 1141.08 1465.67 1142.01 C 1465.39 1142.94 1464.5 1143.44 1463.67 1143.13 C 1453.82 1140.7 1443.62 1140.7 1433.77 1143.13 C 1433.15 1142.95 1432.66 1142.43 1432.46 1141.75 C 1432.25 1141.07 1432.38 1140.33 1432.78 1139.77 C 1443.26 1137 1454.18 1137 1464.67 1139.77" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(180,1449,1158.5)"/>
        <ellipse cx="1359" cy="1110" rx="140" ry="50" fill="#ffffff" stroke="#000000"/>
        <rect x="300" y="1007" width="80" height="5" fill="#ffffff" stroke="#000000"/>
        <path d="M 300 1012 C 300 1056.18 335.82 1092 380 1092 L 380 1012" fill="none" stroke="#000000" strokeMiterlimit="10"/>
        <rect x="403" y="1008" width="80" height="5" fill="#ffffff" stroke="#000000"/>
        <path d="M 483 1013 C 483 1057.18 447.18 1093 403 1093 L 403 1013" fill="none" stroke="#000000" strokeMiterlimit="10"/>
        <rect x="851" y="1008" width="80" height="5" fill="#ffffff" stroke="#000000"/>
        <path d="M 931 1013 C 931 1057.18 895.18 1093 851 1093 L 851 1013" fill="none" stroke="#000000" strokeMiterlimit="10"/>
        <path d="M 0 385 L 0 0 L 100 0 L 100 10 L 10 10 L 10 385 Z" fill="#000000" stroke="#000000" strokeMiterlimit="10"/>
        <path d="M 187 1383 L 187 -173 L 1397 -173 L 1397 1383 L 1387 1383 L 1387 -163 L 197 -163 L 197 1383 Z" fill="#000000" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,792,605)"/>
        <rect x="-35" y="423" width="160" height="5" fill="#ffffff" stroke="#000000" transform="rotate(-90,45,465.5)"/>
        <path d="M 45 423 L 45 428 M 45 428 C 45 472.18 9.18 508 -35 508 L -35 428 M 45 428 C 45 472.18 80.82 508 125 508 L 125 428" fill="none" stroke="#000000" strokeMiterlimit="10" transform="rotate(-90,45,465.5)"/>
        <rect x="1" y="781" width="204" height="10" fill="#000000" stroke="#000000"/>
        <rect x="82" y="787" width="246" height="10" fill="#000000" stroke="#000000" transform="rotate(90,205,792)"/>
        <path d="M 99 682 L 99 477 L 116 477 L 116 487 L 109 487 L 109 682 Z" fill="#000000" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,107.5,579.5)"/>
        <rect x="124" y="586" width="80" height="5" fill="#ffffff" stroke="#000000" transform="rotate(90,164,628.5)"/>
        <path d="M 204 591 C 204 635.18 168.18 671 124 671 L 124 591" fill="none" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,164,628.5)"/>
        <rect x="117" y="698" width="100" height="60" fill="#ffffff" stroke="#000000" transform="rotate(90,167,730.5)"/>
        <path d="M 117 753 L 217 753 M 167 753 L 167 758" fill="none" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,167,730.5)"/>
        <rect x="137" y="758" width="10" height="5" fill="#ffffff" stroke="#000000" transform="rotate(90,167,730.5)"/>
        <rect x="187" y="758" width="10" height="5" fill="#ffffff" stroke="#000000" transform="rotate(90,167,730.5)"/>
        <rect x="1" y="1005" width="299" height="10" fill="#000000" stroke="#000000"/>
        <rect x="196" y="1000" width="18" height="10" fill="#000000" stroke="#000000" transform="rotate(90,205,1005)"/>
        <rect x="124" y="913" width="80" height="5" fill="#ffffff" stroke="#000000" transform="translate(164,0)scale(-1,1)translate(-164,0)rotate(-90,164,955.5)"/>
        <path d="M 204 918 C 204 962.18 168.18 998 124 998 L 124 918" fill="none" stroke="#000000" strokeMiterlimit="10" transform="translate(164,0)scale(-1,1)translate(-164,0)rotate(-90,164,955.5)"/>
        <rect x="117" y="808" width="100" height="60" fill="#ffffff" stroke="#000000" transform="rotate(90,167,840.5)"/>
        <path d="M 117 863 L 217 863 M 167 863 L 167 868" fill="none" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,167,840.5)"/>
        <rect x="137" y="868" width="10" height="5" fill="#ffffff" stroke="#000000" transform="rotate(90,167,840.5)"/>
        <rect x="187" y="868" width="10" height="5" fill="#ffffff" stroke="#000000" transform="rotate(90,167,840.5)"/>
        <rect x="990" y="522" width="70" height="5" fill="#ffffff" stroke="#000000" transform="rotate(90,1025,522)"/>
        <path d="M 1000 522 L 1007 518 C 1007.91 517.44 1008.94 517.1 1010 517 L 1040 517 C 1041.06 517.1 1042.09 517.44 1043 518 L 1050 522 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,1025,522)"/>
        <path d="M 1019 518 L 1019 522 M 1022 518 L 1022 522 M 1025 518 L 1025 522 M 1031 518 L 1031 522 M 1028 518 L 1028 522" fill="none" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,1025,522)"/>
        <rect x="990" y="703" width="70" height="5" fill="#ffffff" stroke="#000000" transform="rotate(90,1025,703)"/>
        <path d="M 1000 703 L 1007 699 C 1007.91 698.44 1008.94 698.1 1010 698 L 1040 698 C 1041.06 698.1 1042.09 698.44 1043 699 L 1050 703 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,1025,703)"/>
        <path d="M 1019 699 L 1019 703 M 1022 699 L 1022 703 M 1025 699 L 1025 703 M 1031 699 L 1031 703 M 1028 699 L 1028 703" fill="none" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,1025,703)"/>
        <rect x="-327" y="873" width="664" height="10" fill="#000000" stroke="#000000" transform="rotate(90,5,878)"/>
        <rect x="296.5" y="1104.5" width="191" height="10" fill="#000000" stroke="#000000" transform="rotate(90,392,1109.5)"/>
        <rect x="381" y="1005" width="22" height="10" fill="#000000" stroke="#000000"/>
        <rect x="-7" y="1033" width="100" height="60" fill="#ffffff" stroke="#000000" transform="translate(0,1065.5)scale(1,-1)translate(0,-1065.5)rotate(-90,43,1065.5)"/>
        <path d="M -7 1088 L 93 1088 M 43 1088 L 43 1093" fill="none" stroke="#000000" strokeMiterlimit="10" transform="translate(0,1065.5)scale(1,-1)translate(0,-1065.5)rotate(-90,43,1065.5)"/>
        <rect x="13" y="1093" width="10" height="5" fill="#ffffff" stroke="#000000" transform="translate(0,1065.5)scale(1,-1)translate(0,-1065.5)rotate(-90,43,1065.5)"/>
        <rect x="63" y="1093" width="10" height="5" fill="#ffffff" stroke="#000000" transform="translate(0,1065.5)scale(1,-1)translate(0,-1065.5)rotate(-90,43,1065.5)"/>
        <rect x="519.5" y="1104.5" width="191" height="10" fill="#000000" stroke="#000000" transform="rotate(90,615,1109.5)"/>
        <rect x="483" y="1005" width="144" height="10" fill="#000000" stroke="#000000"/>
        <rect x="627" y="1008" width="80" height="5" fill="#ffffff" stroke="#000000"/>
        <path d="M 707 1013 C 707 1057.18 671.18 1093 627 1093 L 627 1013" fill="none" stroke="#000000" strokeMiterlimit="10"/>
        <rect x="744.5" y="1105.5" width="191" height="10" fill="#000000" stroke="#000000" transform="rotate(90,840,1110.5)"/>
        <rect x="707" y="1005" width="144" height="10" fill="#000000" stroke="#000000"/>
        <rect x="931" y="1005" width="129" height="10" fill="#000000" stroke="#000000"/>
        <rect x="510" y="1015" width="100" height="60" fill="#ffffff" stroke="#000000"/>
        <path d="M 510 1070 L 610 1070 M 560 1070 L 560 1075" fill="none" stroke="#000000" strokeMiterlimit="10"/>
        <rect x="530" y="1075" width="10" height="5" fill="#ffffff" stroke="#000000"/>
        <rect x="580" y="1075" width="10" height="5" fill="#ffffff" stroke="#000000"/>
        <rect x="734" y="1015" width="100" height="60" fill="#ffffff" stroke="#000000"/>
        <path d="M 734 1070 L 834 1070 M 784 1070 L 784 1075" fill="none" stroke="#000000" strokeMiterlimit="10"/>
        <rect x="754" y="1075" width="10" height="5" fill="#ffffff" stroke="#000000"/>
        <rect x="804" y="1075" width="10" height="5" fill="#ffffff" stroke="#000000"/>
        <rect x="960" y="1015" width="100" height="60" fill="#ffffff" stroke="#000000"/>
        <path d="M 960 1070 L 1060 1070 M 1010 1070 L 1010 1075" fill="none" stroke="#000000" strokeMiterlimit="10"/>
        <rect x="980" y="1075" width="10" height="5" fill="#ffffff" stroke="#000000"/>
        <rect x="1030" y="1075" width="10" height="5" fill="#ffffff" stroke="#000000"/>
        <path d="M 1059 1204 L 1059 954 L 1080 954 L 1080 964 L 1069 964 L 1069 1204 Z" fill="#000000" stroke="#000000" strokeMiterlimit="10"/>
        <path d="M 1278 1246 L 1278 916 L 1532 916 L 1532 926 L 1288 926 L 1288 1246 Z" fill="#000000" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,1405,1081)"/>
        <rect x="1080" y="957" width="160" height="5" fill="#ffffff" stroke="#000000"/>
        <path d="M 1160 957 L 1160 962 M 1160 962 C 1160 1006.18 1124.18 1042 1080 1042 L 1080 962 M 1160 962 C 1160 1006.18 1195.82 1042 1240 1042 L 1240 962" fill="none" stroke="#000000" strokeMiterlimit="10"/>
        <rect x="1260" y="954" width="120" height="10" fill="#ffffff" stroke="#000000"/>
        <path d="M 1260 959 L 1380 959" fill="none" stroke="#000000" strokeMiterlimit="10"/>
        <rect x="1400" y="954" width="110" height="10" fill="#ffffff" stroke="#000000"/>
        <path d="M 1400 959 L 1510 959" fill="none" stroke="#000000" strokeMiterlimit="10"/>
        <rect x="1570" y="1005" width="10" height="10" fill="#000000" stroke="#000000"/>
        <path d="M 8 376 L 8 364 L 210 364 L 210 374 L 18 374 L 18 376 Z" fill="#000000" stroke="#000000" strokeMiterlimit="10" transform="rotate(180,109,370)"/>
        <rect x="65.5" y="138.5" width="279" height="10" fill="#000000" stroke="#000000" transform="rotate(90,205,143.5)"/>
        <rect x="125" y="281" width="80" height="5" fill="#ffffff" stroke="#000000" transform="translate(165,0)scale(-1,1)translate(-165,0)rotate(-90,165,323.5)"/>
        <path d="M 205 286 C 205 330.18 169.18 366 125 366 L 125 286" fill="none" stroke="#000000" strokeMiterlimit="10" transform="translate(165,0)scale(-1,1)translate(-165,0)rotate(-90,165,323.5)"/>
        <rect x="210" y="240" width="212" height="10" fill="#000000" stroke="#000000"/>
        <path d="M 382.44 69.28 L 385.43 93.36 C 386 96.89 384.44 100.44 381.45 102.38 C 371.21 104 360.79 104 350.55 102.38 C 347.56 100.44 346 96.89 346.57 93.36 L 349.56 69.28 C 350.13 66.61 352.98 64.56 356.53 64.26 L 375.47 64.26 C 379.02 64.56 381.87 66.61 382.44 69.28 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,366,82.5)"/>
        <path d="M 381.67 63.77 C 382.49 64.08 382.94 65.08 382.67 66.01 C 382.39 66.94 381.5 67.44 380.67 67.13 C 370.82 64.7 360.62 64.7 350.77 67.13 C 350.15 66.95 349.66 66.43 349.46 65.75 C 349.25 65.07 349.38 64.33 349.78 63.77 C 360.26 61 371.18 61 381.67 63.77" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,366,82.5)"/>
        <path d="M 285.44 69.28 L 288.43 93.36 C 289 96.89 287.44 100.44 284.45 102.38 C 274.21 104 263.79 104 253.55 102.38 C 250.56 100.44 249 96.89 249.57 93.36 L 252.56 69.28 C 253.13 66.61 255.98 64.56 259.53 64.26 L 278.47 64.26 C 282.02 64.56 284.87 66.61 285.44 69.28 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(270,269,82.5)"/>
        <path d="M 284.67 63.77 C 285.49 64.08 285.94 65.08 285.67 66.01 C 285.39 66.94 284.5 67.44 283.67 67.13 C 273.82 64.7 263.62 64.7 253.77 67.13 C 253.15 66.95 252.66 66.43 252.46 65.75 C 252.25 65.07 252.38 64.33 252.78 63.77 C 263.26 61 274.18 61 284.67 63.77" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(270,269,82.5)"/>
        <path d="M 382.44 129.28 L 385.43 153.36 C 386 156.89 384.44 160.44 381.45 162.38 C 371.21 164 360.79 164 350.55 162.38 C 347.56 160.44 346 156.89 346.57 153.36 L 349.56 129.28 C 350.13 126.61 352.98 124.56 356.53 124.26 L 375.47 124.26 C 379.02 124.56 381.87 126.61 382.44 129.28 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,366,142.5)"/>
        <path d="M 381.67 123.77 C 382.49 124.08 382.94 125.08 382.67 126.01 C 382.39 126.94 381.5 127.44 380.67 127.13 C 370.82 124.7 360.62 124.7 350.77 127.13 C 350.15 126.95 349.66 126.43 349.46 125.75 C 349.25 125.07 349.38 124.33 349.78 123.77 C 360.26 121 371.18 121 381.67 123.77" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,366,142.5)"/>
        <path d="M 285.44 129.28 L 288.43 153.36 C 289 156.89 287.44 160.44 284.45 162.38 C 274.21 164 263.79 164 253.55 162.38 C 250.56 160.44 249 156.89 249.57 153.36 L 252.56 129.28 C 253.13 126.61 255.98 124.56 259.53 124.26 L 278.47 124.26 C 282.02 124.56 284.87 126.61 285.44 129.28 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(270,269,142.5)"/>
        <path d="M 284.67 123.77 C 285.49 124.08 285.94 125.08 285.67 126.01 C 285.39 126.94 284.5 127.44 283.67 127.13 C 273.82 124.7 263.62 124.7 253.77 127.13 C 253.15 126.95 252.66 126.43 252.46 125.75 C 252.25 125.07 252.38 124.33 252.78 123.77 C 263.26 121 274.18 121 284.67 123.77" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(270,269,142.5)"/>
        <rect x="237" y="62" width="160" height="100" fill="#ffffff" stroke="#000000" transform="rotate(90,317,112)"/>
        <rect x="116" y="136" width="100" height="60" fill="#ffffff" stroke="#000000" transform="rotate(90,166,168.5)"/>
        <path d="M 116 191 L 216 191 M 166 191 L 166 196" fill="none" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,166,168.5)"/>
        <rect x="136" y="196" width="10" height="5" fill="#ffffff" stroke="#000000" transform="rotate(90,166,168.5)"/>
        <rect x="186" y="196" width="10" height="5" fill="#ffffff" stroke="#000000" transform="rotate(90,166,168.5)"/>
        <path d="M 755.44 74.28 L 758.43 98.36 C 759 101.89 757.44 105.44 754.45 107.38 C 744.21 109 733.79 109 723.55 107.38 C 720.56 105.44 719 101.89 719.57 98.36 L 722.56 74.28 C 723.13 71.61 725.98 69.56 729.53 69.26 L 748.47 69.26 C 752.02 69.56 754.87 71.61 755.44 74.28 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,739,87.5)"/>
        <path d="M 754.67 68.77 C 755.49 69.08 755.94 70.08 755.67 71.01 C 755.39 71.94 754.5 72.44 753.67 72.13 C 743.82 69.7 733.62 69.7 723.77 72.13 C 723.15 71.95 722.66 71.43 722.46 70.75 C 722.25 70.07 722.38 69.33 722.78 68.77 C 733.26 66 744.18 66 754.67 68.77" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,739,87.5)"/>
        <path d="M 658.44 74.28 L 661.43 98.36 C 662 101.89 660.44 105.44 657.45 107.38 C 647.21 109 636.79 109 626.55 107.38 C 623.56 105.44 622 101.89 622.57 98.36 L 625.56 74.28 C 626.13 71.61 628.98 69.56 632.53 69.26 L 651.47 69.26 C 655.02 69.56 657.87 71.61 658.44 74.28 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(270,642,87.5)"/>
        <path d="M 657.67 68.77 C 658.49 69.08 658.94 70.08 658.67 71.01 C 658.39 71.94 657.5 72.44 656.67 72.13 C 646.82 69.7 636.62 69.7 626.77 72.13 C 626.15 71.95 625.66 71.43 625.46 70.75 C 625.25 70.07 625.38 69.33 625.78 68.77 C 636.26 66 647.18 66 657.67 68.77" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(270,642,87.5)"/>
        <path d="M 755.44 134.28 L 758.43 158.36 C 759 161.89 757.44 165.44 754.45 167.38 C 744.21 169 733.79 169 723.55 167.38 C 720.56 165.44 719 161.89 719.57 158.36 L 722.56 134.28 C 723.13 131.61 725.98 129.56 729.53 129.26 L 748.47 129.26 C 752.02 129.56 754.87 131.61 755.44 134.28 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,739,147.5)"/>
        <path d="M 754.67 128.77 C 755.49 129.08 755.94 130.08 755.67 131.01 C 755.39 131.94 754.5 132.44 753.67 132.13 C 743.82 129.7 733.62 129.7 723.77 132.13 C 723.15 131.95 722.66 131.43 722.46 130.75 C 722.25 130.07 722.38 129.33 722.78 128.77 C 733.26 126 744.18 126 754.67 128.77" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,739,147.5)"/>
        <path d="M 658.44 134.28 L 661.43 158.36 C 662 161.89 660.44 165.44 657.45 167.38 C 647.21 169 636.79 169 626.55 167.38 C 623.56 165.44 622 161.89 622.57 158.36 L 625.56 134.28 C 626.13 131.61 628.98 129.56 632.53 129.26 L 651.47 129.26 C 655.02 129.56 657.87 131.61 658.44 134.28 Z" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(270,642,147.5)"/>
        <path d="M 657.67 128.77 C 658.49 129.08 658.94 130.08 658.67 131.01 C 658.39 131.94 657.5 132.44 656.67 132.13 C 646.82 129.7 636.62 129.7 626.77 132.13 C 626.15 131.95 625.66 131.43 625.46 130.75 C 625.25 130.07 625.38 129.33 625.78 128.77 C 636.26 126 647.18 126 657.67 128.77" fill="#ffffff" stroke="#000000" strokeMiterlimit="10" transform="rotate(270,642,147.5)"/>
        <rect x="610" y="67" width="160" height="100" fill="#ffffff" stroke="#000000" transform="rotate(90,690,117)"/>
        <rect x="423" y="162" width="160" height="5" fill="#ffffff" stroke="#000000" transform="translate(0,204.5)scale(1,-1)translate(0,-204.5)"/>
        <path d="M 503 162 L 503 167 M 503 167 C 503 211.18 467.18 247 423 247 L 423 167 M 503 167 C 503 211.18 538.82 247 583 247 L 583 167" fill="none" stroke="#000000" strokeMiterlimit="10" transform="translate(0,204.5)scale(1,-1)translate(0,-204.5)"/>
        <rect x="1480" y="240" width="87" height="10" fill="#000000" stroke="#000000"/>
        <rect x="1400" y="162" width="80" height="5" fill="#ffffff" stroke="#000000" transform="translate(0,204.5)scale(1,-1)translate(0,-204.5)"/>
        <path d="M 1400 167 C 1400 211.18 1435.82 247 1480 247 L 1480 167" fill="none" stroke="#000000" strokeMiterlimit="10" transform="translate(0,204.5)scale(1,-1)translate(0,-204.5)"/>
        <rect x="1260" y="240" width="140" height="10" fill="#000000" stroke="#000000"/>
        <rect x="1148" y="121" width="244" height="10" fill="#000000" stroke="#000000" transform="rotate(90,1270,126)"/>
        <rect x="1258" y="157" width="100" height="60" fill="#ffffff" stroke="#000000" transform="rotate(-90,1308,189.5)"/>
        <path d="M 1258 212 L 1358 212 M 1308 212 L 1308 217" fill="none" stroke="#000000" strokeMiterlimit="10" transform="rotate(-90,1308,189.5)"/>
        <rect x="1278" y="217" width="10" height="5" fill="#ffffff" stroke="#000000" transform="rotate(-90,1308,189.5)"/>
        <rect x="1328" y="217" width="10" height="5" fill="#ffffff" stroke="#000000" transform="rotate(-90,1308,189.5)"/>
        <rect x="1180" y="162" width="80" height="5" fill="#ffffff" stroke="#000000" transform="translate(0,204.5)scale(1,-1)translate(0,-204.5)"/>
        <path d="M 1180 167 C 1180 211.18 1215.82 247 1260 247 L 1260 167" fill="none" stroke="#000000" strokeMiterlimit="10" transform="translate(0,204.5)scale(1,-1)translate(0,-204.5)"/>
        <rect x="1040" y="240" width="140" height="10" fill="#000000" stroke="#000000"/>
        <rect x="928" y="121" width="244" height="10" fill="#000000" stroke="#000000" transform="rotate(90,1050,126)"/>
        <rect x="1038" y="157" width="100" height="60" fill="#ffffff" stroke="#000000" transform="rotate(-90,1088,189.5)"/>
        <path d="M 1038 212 L 1138 212 M 1088 212 L 1088 217" fill="none" stroke="#000000" strokeMiterlimit="10" transform="rotate(-90,1088,189.5)"/>
        <rect x="1058" y="217" width="10" height="5" fill="#ffffff" stroke="#000000" transform="rotate(-90,1088,189.5)"/>
        <rect x="1108" y="217" width="10" height="5" fill="#ffffff" stroke="#000000" transform="rotate(-90,1088,189.5)"/>
        <rect x="959" y="162" width="80" height="5" fill="#ffffff" stroke="#000000" transform="translate(0,204.5)scale(1,-1)translate(0,-204.5)"/>
        <path d="M 959 167 C 959 211.18 994.82 247 1039 247 L 1039 167" fill="none" stroke="#000000" strokeMiterlimit="10" transform="translate(0,204.5)scale(1,-1)translate(0,-204.5)"/>
        <rect x="583" y="240" width="375" height="10" fill="#000000" stroke="#000000"/>
        <rect x="707" y="121" width="244" height="10" fill="#000000" stroke="#000000" transform="rotate(90,829,126)"/>
        <rect x="817" y="157" width="100" height="60" fill="#ffffff" stroke="#000000" transform="rotate(-90,867,189.5)"/>
        <path d="M 817 212 L 917 212 M 867 212 L 867 217" fill="none" stroke="#000000" strokeMiterlimit="10" transform="rotate(-90,867,189.5)"/>
        <rect x="837" y="217" width="10" height="5" fill="#ffffff" stroke="#000000" transform="rotate(-90,867,189.5)"/>
        <rect x="887" y="217" width="10" height="5" fill="#ffffff" stroke="#000000" transform="rotate(-90,867,189.5)"/>
        <rect x="1062" y="425" width="50" height="15" rx="2" ry="2" fill="#ffffff" stroke="#000000" transform="rotate(-90,1087,458.5)"/>
        <rect x="1071" y="440" width="32" height="17" fill="#ffffff" stroke="#000000" transform="rotate(-90,1087,458.5)"/>
        <ellipse cx="1087" cy="467" rx="20" ry="25" fill="#ffffff" stroke="#000000" transform="rotate(-90,1087,458.5)"/>
        <ellipse cx="1087" cy="469" rx="12" ry="15" fill="none" stroke="#000000" transform="rotate(-90,1087,458.5)"/>
        <ellipse cx="1087" cy="461" rx="5" ry="7" fill="#000000" stroke="none" transform="rotate(-90,1087,458.5)"/>
        <rect x="850.5" y="603.5" width="399" height="10" fill="#000000" stroke="#000000" transform="rotate(90,1050,608.5)"/>
        <rect x="1064" y="523" width="50" height="15" rx="2" ry="2" fill="#ffffff" stroke="#000000" transform="rotate(-90,1089,556.5)"/>
        <rect x="1073" y="538" width="32" height="17" fill="#ffffff" stroke="#000000" transform="rotate(-90,1089,556.5)"/>
        <ellipse cx="1089" cy="565" rx="20" ry="25" fill="#ffffff" stroke="#000000" transform="rotate(-90,1089,556.5)"/>
        <ellipse cx="1089" cy="567" rx="12" ry="15" fill="none" stroke="#000000" transform="rotate(-90,1089,556.5)"/>
        <ellipse cx="1089" cy="559" rx="5" ry="7" fill="#000000" stroke="none" transform="rotate(-90,1089,556.5)"/>
        <rect x="1055" y="502" width="172" height="10" fill="#000000" stroke="#000000"/>
        <rect x="1153" y="415" width="80" height="5" fill="#ffffff" stroke="#000000" transform="rotate(90,1193,457.5)"/>
        <path d="M 1233 420 C 1233 464.18 1197.18 500 1153 500 L 1153 420" fill="none" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,1193,457.5)"/>
        <rect x="1153" y="514" width="80" height="5" fill="#ffffff" stroke="#000000" transform="rotate(90,1193,556.5)"/>
        <path d="M 1233 519 C 1233 563.18 1197.18 599 1153 599 L 1153 519" fill="none" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,1193,556.5)"/>
        <path d="M 1048 611 L 1048 597 L 1371 597 L 1371 607 L 1058 607 L 1058 611 Z" fill="#000000" stroke="#000000" strokeMiterlimit="10" transform="rotate(180,1209.5,604)"/>
        <rect x="1223" y="502" width="18" height="10" fill="#000000" stroke="#000000" transform="rotate(90,1232,507)"/>
        <rect x="1227.5" y="407.5" width="9" height="10" fill="#000000" stroke="#000000" transform="rotate(90,1232,412.5)"/>
        <rect x="1323" y="420" width="40" height="35" rx="4" ry="4" fill="#ffffff" stroke="#000000" transform="rotate(90,1343,437.5)"/>
        <rect x="1327" y="430" width="32" height="21" rx="3" ry="3" fill="none" stroke="#000000" transform="rotate(90,1343,437.5)"/>
        <ellipse cx="1343" cy="439" rx="2.5" ry="2.5" fill="#000000" stroke="none" transform="rotate(90,1343,437.5)"/>
        <rect x="1323" y="420" width="0" height="0" fill="none" stroke="#000000" transform="rotate(90,1343,437.5)"/>
        <rect x="1341" y="423" width="4" height="16" rx="2" ry="2" fill="#ffffff" stroke="#000000" transform="rotate(90,1343,437.5)"/>
        <rect x="1341" y="423" width="4" height="11" rx="2" ry="2" fill="none" stroke="#000000" transform="rotate(90,1343,437.5)"/>
        <rect x="1323" y="464" width="40" height="35" rx="4" ry="4" fill="#ffffff" stroke="#000000" transform="rotate(90,1343,481.5)"/>
        <rect x="1327" y="474" width="32" height="21" rx="3" ry="3" fill="none" stroke="#000000" transform="rotate(90,1343,481.5)"/>
        <ellipse cx="1343" cy="483" rx="2.5" ry="2.5" fill="#000000" stroke="none" transform="rotate(90,1343,481.5)"/>
        <rect x="1323" y="464" width="0" height="0" fill="none" stroke="#000000" transform="rotate(90,1343,481.5)"/>
        <rect x="1341" y="467" width="4" height="16" rx="2" ry="2" fill="#ffffff" stroke="#000000" transform="rotate(90,1343,481.5)"/>
        <rect x="1341" y="467" width="4" height="11" rx="2" ry="2" fill="none" stroke="#000000" transform="rotate(90,1343,481.5)"/>
        <rect x="1286" y="514" width="80" height="5" fill="#ffffff" stroke="#000000" transform="rotate(90,1326,556.5)"/>
        <path d="M 1286 519 C 1286 563.18 1321.82 599 1366 599 L 1366 519" fill="none" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,1326,556.5)"/>
        <path d="M 1151 622 L 1151 296 L 1265 296 L 1265 306 L 1161 306 L 1161 622 Z" fill="#000000" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,1208,459)"/>
        <rect x="1152" y="611" width="80" height="5" fill="#ffffff" stroke="#000000" transform="rotate(90,1192,653.5)"/>
        <path d="M 1232 616 C 1232 660.18 1196.18 696 1152 696 L 1152 616" fill="none" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,1192,653.5)"/>
        <rect x="1224" y="600" width="16" height="10" fill="#000000" stroke="#000000" transform="rotate(90,1232,605)"/>
        <rect x="1048" y="697" width="180" height="10" fill="#000000" stroke="#000000"/>
        <rect x="1223" y="698" width="18" height="10" fill="#000000" stroke="#000000" transform="rotate(90,1232,703)"/>
        <rect x="1152" y="710" width="80" height="5" fill="#ffffff" stroke="#000000" transform="rotate(90,1192,752.5)"/>
        <path d="M 1232 715 C 1232 759.18 1196.18 795 1152 795 L 1152 715" fill="none" stroke="#000000" strokeMiterlimit="10" transform="rotate(90,1192,752.5)"/>
        <rect x="1227" y="788" width="10" height="10" fill="#000000" stroke="#000000" transform="rotate(90,1232,793)"/>
        <path d="M 1048 806 L 1048 696 L 1371 696 L 1371 706 L 1058 706 L 1058 806 Z" fill="#000000" stroke="#000000" strokeMiterlimit="10" transform="rotate(180,1209.5,751)"/>
        <rect x="1064" y="621" width="50" height="15" rx="2" ry="2" fill="#ffffff" stroke="#000000" transform="rotate(-90,1089,654.5)"/>
        <rect x="1073" y="636" width="32" height="17" fill="#ffffff" stroke="#000000" transform="rotate(-90,1089,654.5)"/>
        <ellipse cx="1089" cy="663" rx="20" ry="25" fill="#ffffff" stroke="#000000" transform="rotate(-90,1089,654.5)"/>
        <ellipse cx="1089" cy="665" rx="12" ry="15" fill="none" stroke="#000000" transform="rotate(-90,1089,654.5)"/>
        <ellipse cx="1089" cy="657" rx="5" ry="7" fill="#000000" stroke="none" transform="rotate(-90,1089,654.5)"/>
        <rect x="1064" y="720" width="50" height="15" rx="2" ry="2" fill="#ffffff" stroke="#000000" transform="rotate(-90,1089,753.5)"/>
        <rect x="1073" y="735" width="32" height="17" fill="#ffffff" stroke="#000000" transform="rotate(-90,1089,753.5)"/>
        <ellipse cx="1089" cy="762" rx="20" ry="25" fill="#ffffff" stroke="#000000" transform="rotate(-90,1089,753.5)"/>
        <ellipse cx="1089" cy="764" rx="12" ry="15" fill="none" stroke="#000000" transform="rotate(-90,1089,753.5)"/>
        <ellipse cx="1089" cy="756" rx="5" ry="7" fill="#000000" stroke="none" transform="rotate(-90,1089,753.5)"/>
        <rect x="1323" y="713" width="40" height="35" rx="4" ry="4" fill="#ffffff" stroke="#000000" transform="rotate(90,1343,730.5)"/>
        <rect x="1327" y="723" width="32" height="21" rx="3" ry="3" fill="none" stroke="#000000" transform="rotate(90,1343,730.5)"/>
        <ellipse cx="1343" cy="732" rx="2.5" ry="2.5" fill="#000000" stroke="none" transform="rotate(90,1343,730.5)"/>
        <rect x="1323" y="713" width="0" height="0" fill="none" stroke="#000000" transform="rotate(90,1343,730.5)"/>
        <rect x="1341" y="716" width="4" height="16" rx="2" ry="2" fill="#ffffff" stroke="#000000" transform="rotate(90,1343,730.5)"/>
        <rect x="1341" y="716" width="4" height="11" rx="2" ry="2" fill="none" stroke="#000000" transform="rotate(90,1343,730.5)"/>
        <rect x="1323" y="757" width="40" height="35" rx="4" ry="4" fill="#ffffff" stroke="#000000" transform="rotate(90,1343,774.5)"/>
        <rect x="1327" y="767" width="32" height="21" rx="3" ry="3" fill="none" stroke="#000000" transform="rotate(90,1343,774.5)"/>
        <ellipse cx="1343" cy="776" rx="2.5" ry="2.5" fill="#000000" stroke="none" transform="rotate(90,1343,774.5)"/>
        <rect x="1323" y="757" width="0" height="0" fill="none" stroke="#000000" transform="rotate(90,1343,774.5)"/>
        <rect x="1341" y="760" width="4" height="16" rx="2" ry="2" fill="#ffffff" stroke="#000000" transform="rotate(90,1343,774.5)"/>
        <rect x="1341" y="760" width="4" height="11" rx="2" ry="2" fill="none" stroke="#000000" transform="rotate(90,1343,774.5)"/>
        <rect x="1286" y="613" width="80" height="5" fill="#ffffff" stroke="#000000" transform="translate(1326,0)scale(-1,1)translate(-1326,0)rotate(-90,1326,655.5)"/>
        <path d="M 1286 618 C 1286 662.18 1321.82 698 1366 698 L 1366 618" fill="none" stroke="#000000" strokeMiterlimit="10" transform="translate(1326,0)scale(-1,1)translate(-1326,0)rotate(-90,1326,655.5)"/>
        <rect x="1361" y="605" width="10" height="10" fill={this.state.fill_a} stroke="#000000" transform="rotate(90,1366,610)"/>
        <rect x="227" y="277" width="762" height="700" fill={this.state.fill_b} stroke="none"/>
    </g>
</svg>

    </div>
          <div>

<svg version="1.1" id="Layer_1" xmlns="http://www.w3.org/2000/svg" xmlnsXlink="http://www.w3.org/1999/xlink" x="0px" y="0px"
	 viewBox="0 0 517 163" style={{enableBackground:'new 0 0 517 163'}} xmlSpace="preserve">
<g>
	<path fill={this.state.figures[0]} d="m 64.7 127.83 c 1.14 -16.72 2.23 -33.36 3.38 -50.11 l 8.7 12.97 c 1.78 2.33 8.26 -1.28 7.12 -3.86 c -3.52 -8.86 -9.95 -24.38 -13.67 -33.71 c -0.29 -0.61 -0.68 -1.12 -1.22 -1.47 c -9.15 -5.03 -26.49 -5.03 -35.66 -0.01 c -0.59 0.34 -1.04 0.81 -1.29 1.44 c -3.71 9.32 -10.18 24.91 -13.7 33.8 c -1.14 2.58 5.34 6.2 7.12 3.86 l 8.7 -12.97 c 1.15 16.68 2.29 33.36 3.44 50.04 c -12.12 1.65 -20.6 5.44 -20.6 9.85 c 0 5.92 15.27 10.73 34.11 10.73 c 18.84 0 34.11 -4.8 34.11 -10.73 c -0 -4.4 -8.46 -8.18 -20.54 -9.83 z"/>
	<path fill={this.state.figures[0]} d="m 49.23 44.71 c 1.71 0.25 2.14 0.25 3.85 0 c 3.53 -0.81 6.72 -3.18 8.09 -5.65 c 1.57 -2.82 2.27 -10.27 1.98 -13.41 c -0.57 -6.03 -5.96 -9.05 -11.99 -9.05 c -6.02 0 -11.42 3.02 -11.99 9.05 c -0.29 3.15 0.41 10.6 1.98 13.41 c 1.36 2.48 4.55 4.84 8.08 5.65 z"/>
</g>
<g>
	<path fill={this.state.figures[1]} d="M147.64,127.83c1.14-16.72,2.23-33.36,3.38-50.11l8.7,12.97c1.78,2.33,8.26-1.28,7.12-3.86
		c-3.52-8.86-9.95-24.38-13.67-33.71c-0.29-0.61-0.68-1.12-1.22-1.47c-9.15-5.03-26.49-5.03-35.66-0.01
		c-0.59,0.34-1.04,0.81-1.29,1.44c-3.71,9.32-10.18,24.91-13.7,33.8c-1.14,2.58,5.34,6.2,7.12,3.86l8.7-12.97
		c1.15,16.68,2.29,33.36,3.44,50.04c-12.12,1.65-20.6,5.44-20.6,9.85c0,5.92,15.27,10.73,34.11,10.73c18.84,0,34.11-4.8,34.11-10.73
		C168.18,133.26,159.73,129.48,147.64,127.83z"/>
	<path fill={this.state.figures[1]} d="M132.18,44.71c1.71,0.25,2.14,0.25,3.85,0c3.53-0.81,6.72-3.18,8.09-5.65c1.57-2.82,2.27-10.27,1.98-13.41
		c-0.57-6.03-5.96-9.05-11.99-9.05c-6.02,0-11.42,3.02-11.99,9.05c-0.29,3.15,0.41,10.6,1.98,13.41
		C125.46,41.54,128.64,43.9,132.18,44.71z"/>
</g>
<g>
	<path fill={this.state.figures[2]} d="M230.59,127.75c1.14-16.69,2.23-33.3,3.38-50.03l8.7,12.97c1.78,2.33,8.26-1.28,7.12-3.86
		c-3.52-8.86-9.95-24.38-13.67-33.71c-0.29-0.61-0.68-1.12-1.22-1.47c-9.15-5.03-26.49-5.03-35.66-0.01
		c-0.59,0.34-1.04,0.81-1.29,1.44c-3.71,9.32-10.18,24.91-13.7,33.8c-1.14,2.58,5.34,6.2,7.12,3.86l8.7-12.97
		c1.15,16.71,2.3,33.42,3.44,50.13c-11.79,1.69-20,5.42-20,9.76c0,5.92,15.27,10.73,34.11,10.73s34.11-4.8,34.11-10.73
		C251.73,133.19,242.99,129.35,230.59,127.75z"/>
	<path fill={this.state.figures[2]} d="M215.12,44.71c1.71,0.25,2.14,0.25,3.85,0c3.53-0.81,6.72-3.18,8.09-5.65c1.57-2.82,2.27-10.27,1.98-13.41
		c-0.57-6.03-5.96-9.05-11.99-9.05c-6.02,0-11.42,3.02-11.99,9.05c-0.29,3.15,0.41,10.6,1.98,13.41
		C208.4,41.54,211.59,43.9,215.12,44.71z"/>
</g>
<g>
	<path fill={this.state.figures[3]} d="M313.54,127.56c1.13-16.63,2.22-33.18,3.37-49.84l8.7,12.97c1.78,2.33,8.26-1.28,7.12-3.86
		c-3.52-8.86-9.95-24.38-13.67-33.71c-0.29-0.61-0.68-1.12-1.22-1.47c-9.15-5.03-26.49-5.03-35.66-0.01
		c-0.59,0.34-1.04,0.81-1.29,1.44c-3.71,9.32-10.18,24.91-13.7,33.8c-1.14,2.58,5.34,6.2,7.12,3.86l8.7-12.97
		c1.15,16.79,2.31,33.57,3.46,50.36c-10.97,1.78-18.48,5.38-18.48,9.53c0,5.92,15.27,10.73,34.11,10.73s34.11-4.8,34.11-10.73
		C336.21,133.01,326.75,129.04,313.54,127.56z"/>
	<path fill={this.state.figures[3]} d="M298.06,44.71c1.71,0.25,2.14,0.25,3.85,0c3.53-0.81,6.72-3.18,8.09-5.65c1.57-2.82,2.27-10.27,1.98-13.41
		c-0.57-6.03-5.96-9.05-11.99-9.05c-6.02,0-11.42,3.02-11.99,9.05c-0.29,3.15,0.41,10.6,1.98,13.41
		C291.34,41.54,294.53,43.9,298.06,44.71z"/>
</g>
<g>
	<path fill={this.state.figures[4]} d="M396.47,127.82c1.14-16.72,2.23-33.35,3.38-50.1l8.7,12.97c1.78,2.33,8.26-1.28,7.12-3.86
		c-3.52-8.86-9.95-24.38-13.67-33.71c-0.29-0.61-0.68-1.12-1.22-1.47c-9.15-5.03-26.49-5.03-35.66-0.01
		c-0.59,0.34-1.04,0.81-1.29,1.44c-3.71,9.32-10.18,24.91-13.7,33.8c-1.14,2.58,5.34,6.2,7.12,3.86l8.7-12.97
		c1.15,16.69,2.29,33.37,3.44,50.05c-12.08,1.65-20.53,5.43-20.53,9.84c0,5.92,15.27,10.73,34.11,10.73s34.11-4.8,34.11-10.73
		C417.08,133.25,408.6,129.46,396.47,127.82z"/>
	<path fill={this.state.figures[4]} d="M381.01,44.71c1.71,0.25,2.14,0.25,3.85,0c3.53-0.81,6.72-3.18,8.09-5.65c1.57-2.82,2.27-10.27,1.98-13.41
		c-0.57-6.03-5.96-9.05-11.99-9.05c-6.02,0-11.42,3.02-11.99,9.05c-0.29,3.15,0.41,10.6,1.98,13.41
		C374.29,41.54,377.48,43.9,381.01,44.71z"/>
</g>
<g>
	<path fill={this.state.figures[5]} d="M479.41,127.82c1.14-16.72,2.23-33.36,3.38-50.1l8.7,12.97c1.78,2.33,8.26-1.28,7.12-3.86
		c-3.52-8.86-9.95-24.38-13.67-33.71c-0.29-0.61-0.68-1.12-1.22-1.47c-9.15-5.03-26.49-5.03-35.66-0.01
		c-0.59,0.34-1.04,0.81-1.29,1.44c-3.71,9.32-10.18,24.91-13.7,33.8c-1.14,2.58,5.34,6.2,7.12,3.86l8.7-12.97
		c1.15,16.68,2.29,33.36,3.44,50.05c-12.1,1.65-20.57,5.43-20.57,9.84c0,5.92,15.27,10.73,34.11,10.73
		c18.84,0,34.11-4.8,34.11-10.73C499.99,133.26,491.52,129.47,479.41,127.82z"/>
	<path fill={this.state.figures[5]} d="M463.95,44.71c1.71,0.25,2.14,0.25,3.85,0c3.53-0.81,6.72-3.18,8.09-5.65c1.57-2.82,2.27-10.27,1.98-13.41
		c-0.57-6.03-5.96-9.05-11.99-9.05c-6.02,0-11.42,3.02-11.99,9.05c-0.29,3.15,0.41,10.6,1.98,13.41
		C457.23,41.54,460.42,43.9,463.95,44.71z"/>
</g>
</svg>

          <p></p>
          <p></p>
          <p></p>
          <p></p>
            </div>
      </div>
    );
  }
}
export default App;
