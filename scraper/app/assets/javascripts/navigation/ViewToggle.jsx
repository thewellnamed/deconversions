var ViewToggle = React.createClass({
	render: function() {
		var browseViewClass = "btn btn-default navbar-btn";
		var codeViewClass = browseViewClass;
		
		if (this.props.selected == 'code') {
			codeViewClass += " active";
		} else {
			browseViewClass += " active";
		} 
		
		return (
            <div className="btn-group navigation-buttons">
				<button type="button" className={browseViewClass} onClick={this.props.onClick.bind(null, "browse")}>
					<span className="glyphicon glyphicon-th"></span>
				</button>
				<button type="button" className={codeViewClass} onClick={this.props.onClick.bind(null, "code")}>
					<span className="glyphicon glyphicon-picture"></span>
				</button>
    		</div>		
		);
	}
});