var ThreadViewer = React.createClass({
	getInitialState: function() {
		return { meta: this.props.meta, data: this.props.thread };
	},

	componentWillReceiveProps: function(newProps) {
		this.setState({ meta: newProps.meta, data: newProps.thread });
	},
	
	render: function() {
		return (<Thread data={this.state.data} />);
	}
});
			
var Thread = React.createClass({
	render: function() {
		return (
			<div className="thread">
			{this.props.data.map(function(entry) {
					return(<ThreadEntry data={entry} />);
			})}
			</div>
		);
	}
})
		
var ThreadEntry = React.createClass({
	render: function() {
		var user = this.props.data.user;
		var content = this.props.data.text;
		return (
			<div className="thread-entry">
				<div className="author">{user}</div>
				<div className="content" dangerouslySetInnerHTML={{__html: content}} />
				<Thread data={this.props.data.children} />
			</div>
		);		
	}
});