var Header = React.createClass({
    handleClick: function(page, e) {
        this.props.onRouteChange(page, e);
    },	
    
	render: function() {
		var email = this.props.account && this.props.account.email ? this.props.account.email : "",
		    login = (<li><a href="#" className="btn" onClick={this.handleClick.bind(null, "login")}>Login</a></li>),
		    logout = (<li><a href="#" className="btn" onClick={this.handleClick.bind(null, "logout")}>Logout</a></li>),
		    log = this.props.session === false ? login : logout,
		    filters = null, viewButtons = null;
		
		if (this.props.account) {
			filters = (<CodeFilters filters={this.props.filters} onFilterChange={this.props.onFilterChange} />);
		}
		
		if (this.props.view == 'code') {
			viewButtons = (
		        <div className="btn-group navigation-buttons">
					<button type="button" className="btn btn-default navbar-btn" onClick={this.handleClick.bind(null, "home")}>
						<span className="glyphicon glyphicon-home"></span>
					</button>
					<button type="button" className="btn btn-default navbar-btn" onClick={this.handleClick.bind(null, "prev")}>
						<span className="glyphicon glyphicon-chevron-left"></span>
					</button>
					<button type="button" className="btn btn-default navbar-btn" onClick={this.handleClick.bind(null, "next")}>
						<span className="glyphicon glyphicon-chevron-right"></span>
					</button>
	    		</div>		
			);
		}
				
		return (
			<nav id="header" className="navbar navbar-default navbar-fixed-top">
                <div className="container-fluid">
                	<div className="navbar-header">
	                    <button type="button" className="navbar-toggle collapsed" data-toggle="collapse" data-target="#nav-collapse" aria-expanded="false">
	                     	<span className="icon-bar"></span>
	                     	<span className="icon-bar"></span>
	                     	<span className="icon-bar"></span>
	                    </button>
                    	<ViewToggle selected={this.props.view} onClick={this.handleClick} />
                		{viewButtons}
                	</div>
                	<div className="collapse navbar-collapse" id="nav-collapse">
                		{filters}
	                	<ul className="nav navbar-nav pull-right">
	                		<li className="navbar-text">{email}</li>
	                		{log}
	                	</ul>
                	</div>
                </div>
           </nav>	
		);
	}
});