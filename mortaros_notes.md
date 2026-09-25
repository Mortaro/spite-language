# Mortaro's inbox

Empty. Add notes here; an agent moves them into `manual.md` and clears this file.

    var json = Json<Order>()
    var text = json.write(order)

should take the object at constructor, unless it causes a problem, then it would be able to infer its type and not need to argue the generic 

    var json = Json(order)
    var text = json.write()