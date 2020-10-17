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
    groups: []
  }

  componentDidMount() {
    fetch('http://192.168.0.154:80/cgi-bin/cgijson.cgi')
    .then(res => res.json())
    .then((data) => {
      this.setState({ rooms: data.rooms })
      this.setState({ assets: data.assets })
      this.setState({ groups: data.groups })
      console.log(this.state)
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
                {/* {this.repeat(beacon, x.beacons)} */}
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
              {x.phones > 0 ? <span>{cellphone}{Math.round(x.phones+.5)}&nbsp;</span> : ''} 
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
      </div>
    );
  }
}
export default App;
