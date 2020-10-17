import React, { Component } from 'react';

class App extends Component {

  state = {
    assets : [],
    areas: [],
    categories: []
  }

  componentDidMount() {
    fetch('http://192.168.0.154:80/cgi-bin/cgijson.cgi')
    .then(res => res.json())
    .then((data) => {
      this.setState({ areas: data.areas })
      this.setState({ assets: data.assets })
      this.setState({ categories: data.categories })
      console.log(this.state)
    })
    .catch(console.log)
  }

  render() {
    return (
     <div className="container">
        <div className="col-xs-12">
        <h1>Assets</h1>
        {this.state.assets.map((x) => (
          <div className="card">
            <div className="card-body">
              <h5 className="card-title">{x.alias}</h5>
              <h6 className="card-subtitle mb-2 text-muted">
              <span> {x.room}</span>
              <span> {x.ago}</span>
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
