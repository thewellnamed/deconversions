var CodeEditor = React.createClass({
	getInitialState: function() {
		return { meta: this.props.meta, data: this.props.codes };
	},
	
	componentWillReceiveProps: function(newProps) {
		this.setState({ meta: newProps.meta, data: newProps.codes });
	},
	
	processCode: function(type, idx, e) {
		var type = parseInt(type);
		var idx = parseInt(idx) - 1;
		var meta = this.state.meta;
		var value = e.target.value;
		
		if (value < 0) {
			this.setState({ data: this.state.data });
			e.preventDefault();
			e.stopPropagation();
			return;
		}
		
		var form = {
			imageId: meta.id,
			type: type,
			seq: idx,
			value: value
		};
		form[this.props.csrfToken.name] = this.props.csrfToken.value;
		
        $.ajax({
            type: 'POST',
            url: '/image/code',
            data: form,
            dataType: 'json',
            cache: false,
            beforeSend : function(xhr) {
            	xhr.setRequestHeader("X-Auth-Token", this.props.session);
            }.bind(this),
            success: function(results) {
                if (results.success) {
            		var data = this.state.data;
            		
                	if (value == 0) {
                		data[type].splice(idx, 1);
                	} else {
	            		if (typeof(data[type]) === "undefined") {
	            			data[type] = [];
	            		}
	            		
	            		data[type][idx] = value;
                	}
                	
            		this.setState({ data: data });
            		this.props.onUpdate(meta.id, data);
                } else {
                	alert('code update failed');
                }
            }.bind(this),
            error: function() {
            	alert('code update failed');
            }.bind(this)
        });
        
        e.preventDefault();
        e.stopPropagation();
	},
	
	render: function() {		
		var numCodesMeta = [1,2,3,4];
		var numCodes = [1,2,3,4,5,6];
		var codes = this.state.data;
		var processCode = this.processCode;	
		
		return (
			<div className="code-editor">
				{CodeTypes.map(function(type) {
					if (type.id == 0) return null;

					var codesForType = (typeof(codes[type.id]) === "undefined") ? [] : codes[type.id];
					var selectedForType = codesForType.reduce(function(selected, next) {
						selected[next] = 1;
						return selected;
					}, {});

					var index = (type.id == CodeType.Meta) ? numCodesMeta : numCodes;
					var codeValues = type.codes;

					return (
						<div className="code-entry-container">
							<div className="code-type">{type.label}</div>
						    <form className="code-form">
						    {index.map(function(idx) {
						    	var currentVal = codesForType.length >= idx ? codesForType[idx-1] : 0;
						    	var enabled = (codesForType.length >= (idx - 1)) ? true : false;
						    	var selectId = "codes-" + type.id + "-" + idx;
						    	return (
						    		<div className="form-group">
					    				{idx}. <select className="code-select" id={selectId} value={currentVal} disabled={!enabled} onChange={processCode.bind(null, type.id, idx)}>
					    	        	{ codeValues.map(function(code) {
					    	        		var label = (currentVal > 0 && code.id == 0) ? "[remove]" : code.label;
					    	        		if (currentVal == code.id || code.id == 0 || typeof(selectedForType[code.id]) === "undefined") {
					    	        			return (<option value={code.id}>{label}</option>);
					    	        		} else {
					    	        			return null;
					    	        		}
					    	        	})}
					    	        	</select>
					    	        </div>
					    	    );
						    })}
						    </form>
					    </div>
				    );
				})}
			</div>
		);
	}
});