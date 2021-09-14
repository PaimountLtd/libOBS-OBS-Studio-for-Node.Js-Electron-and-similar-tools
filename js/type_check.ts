import * as obs from './module'

export function isNumberProperty(property: obs.IProperty): property is obs.INumberProperty {
    return property.type === obs.EPropertyType.Int ||
        property.type === obs.EPropertyType.Float;
}

export function isTextProperty(property: obs.IProperty): property is obs.ITextProperty {
    return property.type === obs.EPropertyType.Text;
}

export function isPathProperty(property: obs.IProperty): property is obs.IPathProperty {
    return property.type === obs.EPropertyType.Path;
}

export function isListProperty(property: obs.IProperty): property is obs.IListProperty {
    return property.type === obs.EPropertyType.List;
}

export function isEditableListProperty(property: obs.IProperty): property is obs.IEditableListProperty {
    return property.type === obs.EPropertyType.EditableList;
}

export function isBooleanProperty(property: obs.IProperty): property is obs.IBooleanProperty {
    return property.type === obs.EPropertyType.Boolean;
}

export function isButtonProperty(property: obs.IProperty): property is obs.IButtonProperty {
    return property.type === obs.EPropertyType.Button;
}

export function isColorProperty(property: obs.IProperty): property is obs.IColorProperty {
    return property.type === obs.EPropertyType.Color;
}

export function isCaptureProperty(property: obs.IProperty): property is obs.ICaptureProperty {
    return property.type === obs.EPropertyType.Capture;
}

export function isFontProperty(property: obs.IProperty): property is obs.IFontProperty {
    return property.type === obs.EPropertyType.Font;
}

/**
 * An empty property is a property defined as having no extra
 * data required to be useful. For instance, a color is just 
 * a color picker. There is no default value to pick from or range allowed.
 */
export function isEmptyProperty(property: obs.IProperty): boolean {
    switch (property.type) {
    case obs.EPropertyType.Boolean:
    case obs.EPropertyType.Button:
    case obs.EPropertyType.Color:
    case obs.EPropertyType.Font:
    case obs.EPropertyType.Invalid:
        return true;
    }

    return false;
}